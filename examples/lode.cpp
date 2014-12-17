#include "luacpp/observable_from_lua.hpp"
#include "luacpp/meta_table.hpp"
#include "luacpp/sink_into_lua.hpp"
#include "luacpp/pcall.hpp"
#include "luacpp/sink_from_lua.hpp"
#include "luacpp/load.hpp"
#include "luacpp/register_async_function.hpp"
#include "luacpp/observable_into_lua.hpp"
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/asio/timer.hpp>
#include <silicium/observable/end.hpp>
#include <silicium/observable/transform.hpp>
#include <silicium/observable/ptr.hpp>
#include <silicium/observable/generator.hpp>
#include <silicium/observable/constant.hpp>
#include <silicium/source/memory_source.hpp>
#include <silicium/source/generator_source.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/optional.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <chrono>

namespace
{
	struct tcp_client : private Si::observer<boost::system::error_code>
	{
		explicit tcp_client(lua::main_thread main_thread, std::shared_ptr<boost::asio::ip::tcp::socket> socket)
			: m_main_thread(main_thread)
			, m_socket(std::move(socket))
			, m_sender(Si::asio::make_writing_observable(*m_socket))
		{
			assert(m_socket);
		}

		void append(Si::fast_variant<lua_Integer, Si::memory_range> element)
		{
			return Si::visit<void>(
				element,
				[this](lua_Integer c)
				{
					m_send_buffer.push_back(static_cast<char>(c));
				},
				[this](Si::memory_range str)
				{
					m_send_buffer.insert(m_send_buffer.end(), str.begin(), str.end());
				}
			);
		}

		void flush(lua::current_thread thread)
		{
			Si::optional<lua::coroutine> coro = lua::pin_coroutine(m_main_thread, thread);
			assert(coro && "you cannot call this function from the Lua main thread");
			m_sender.set_buffer(Si::make_memory_range(m_send_buffer));
			m_sender.async_get_one(Si::observe_by_ref(static_cast<Si::observer<boost::system::error_code> &>(*this)));
			m_coro = std::move(*coro);
			m_coro.suspend();
		}

	private:

		lua::main_thread m_main_thread;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		Si::noexcept_string m_send_buffer;
		Si::asio::writing_observable<boost::asio::ip::tcp::socket> m_sender;
		lua::coroutine m_coro;

		virtual void got_element(boost::system::error_code value) SILICIUM_OVERRIDE
		{
			boost::ignore_unused_variable_warning(value);
			auto coro = std::move(m_coro);
			coro.resume(0);

			boost::system::error_code ignored;
			m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);

			for (;;)
			{
				char dummy_buffer;
				m_socket->read_some(boost::asio::buffer(&dummy_buffer, 1), ignored);
				if (ignored)
				{
					break;
				}
			}
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			SILICIUM_UNREACHABLE();
		}
	};

	inline lua::stack_value create_tcp_client_meta_table(lua::stack &s)
	{
#ifndef NDEBUG
		int initial_stack_size = lua::size(*s.state());
#endif
		lua::stack_value meta = lua::create_default_meta_table<tcp_client>(s);
		assert(lua::size(*s.state()) == initial_stack_size + 1);
		assert(get_type(meta) == lua::type::table);

		add_method(s, meta, "append", &tcp_client::append);
		assert(lua::size(*s.state()) == initial_stack_size + 1);
		assert(get_type(meta) == lua::type::table);

		lua::add_method(s, meta, "flush", &tcp_client::flush);
		assert(lua::size(*s.state()) == initial_stack_size + 1);
		assert(get_type(meta) == lua::type::table);
		return meta;
	}

	struct tcp_acceptor
	{
		typedef Si::asio::tcp_acceptor_result element_type;

		tcp_acceptor()
		{
		}

		tcp_acceptor(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint)
			: m_acceptor(Si::make_unique<boost::asio::ip::tcp::acceptor>(io, endpoint))
			, m_observable(m_acceptor.get())
		{
		}

		template <class Observer>
		void async_get_one(Observer &observer)
		{
			return m_observable.async_get_one(observer);
		}

#if SILICIUM_COMPILER_GENERATES_MOVES
		tcp_acceptor(tcp_acceptor &&other) = default;
		tcp_acceptor &operator = (tcp_acceptor &&other) = default;
#else
		tcp_acceptor(tcp_acceptor &&other)
			: m_acceptor(std::move(other.m_acceptor))
			, m_observable(std::move(other.m_observable))
		{
		}

		tcp_acceptor &operator = (tcp_acceptor &&other)
		{
			m_acceptor = std::move(other.m_acceptor);
			m_observable = std::move(other.m_observable);
			return *this;
		}
#endif

	private:

		std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
		Si::asio::tcp_acceptor<boost::asio::ip::tcp::acceptor *> m_observable;
	};

	struct http_response_generator
	{
		explicit http_response_generator(lua::reference sink)
			: m_sink(std::move(sink))
		{
			assert(m_sink.get_type() == lua::type::user_data);
		}

		void status_line(Si::memory_range status, Si::memory_range status_text, Si::memory_range version, lua_State &state)
		{
			lua::text_sink_into_lua native_sink(m_sink, state);
			Si::http::generate_status_line(native_sink, version, status, status_text);
		}

		void header(Si::memory_range key, Si::memory_range value, lua_State &state)
		{
			lua::text_sink_into_lua native_sink(m_sink, state);
			Si::http::generate_header(native_sink, key, value);
		}

		void content(Si::memory_range content, lua_State &state)
		{
			lua::text_sink_into_lua native_sink(m_sink, state);
			Si::append(native_sink, "\r\n");
			native_sink.append(content);
		}

	private:

		lua::reference m_sink;
	};

	inline std::chrono::microseconds lua_duration_to_cpp(lua_Number duration_seconds)
	{
		std::chrono::microseconds const duration(static_cast<std::int64_t>(duration_seconds * 1000000.0));
		return duration;
	}
	
	lua::stack_value require_package(
		lua::main_thread main_thread,
		lua::stack &stack,
		boost::asio::io_service &io,
		Si::noexcept_string const &name,
		Si::noexcept_string const &version)
	{
		if (name == "tcp" && version == "1.0")
		{
			lua::stack_value module = lua::create_table(*stack.state());
			set_element(
				module,
				"create_acceptor",
				[main_thread, &stack, &io](lua_State &)
			{
				return lua::register_any_function(stack, [main_thread, &io](lua_Integer port, lua_State &L)
				{
					boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), static_cast<boost::uint16_t>(port));
					lua::stack s(L);
					return lua::create_observable(
						L,
						main_thread,
						Si::transform(
							tcp_acceptor(io, endpoint),
							[main_thread](Si::asio::tcp_acceptor_result incoming) -> lua::reference
							{
								if (incoming.is_error())
								{
									return lua::create_reference(main_thread, lua::nil());
								}
								lua::stack s(*main_thread.get());
								lua::stack_value client_meta = create_tcp_client_meta_table(s);
								lua::stack_value client = lua::emplace_object<tcp_client>(s, client_meta, main_thread, incoming.get());
								return lua::create_reference(main_thread, std::move(client));
							}
						)
					);
				});
			});
			module.assert_top();
			return module;
		}
		else if (name == "http" && version == "1.0")
		{
			lua::stack_value module = lua::create_table(*stack.state());
			set_element(
				module,
				"make_response_generator",
				[main_thread, &stack, &io](lua_State &)
			{
				return lua::register_any_function(stack, [main_thread, &io](lua::any_local const &sink, lua_State &L)
				{
					assert(sink.get_type() == lua::type::user_data);
					lua::stack s(L);
					lua::stack_value meta = lua::create_default_meta_table<http_response_generator>(s);
					lua::add_method(s, meta, "status_line", &http_response_generator::status_line);
					lua::add_method(s, meta, "header", &http_response_generator::header);
					lua::add_method(s, meta, "content", &http_response_generator::content);

					lua::reference sink_kept_alive = lua::create_reference(main_thread, sink);
					assert(sink_kept_alive.get_type() == lua::type::user_data);

					lua::stack_value generator = lua::emplace_object<http_response_generator>(s, meta, std::move(sink_kept_alive));
					lua::replace(generator, meta);
					return generator;
				});
			});
			module.assert_top();
			return module;
		}
		else if (name == "gc" && version == "1.0")
		{
			lua::stack_value module = lua::create_table(*stack.state());
			set_element(
				module,
				"get_allocated_bytes",
				[main_thread, &stack](lua_State &)
			{
				return lua::register_any_function(stack, [main_thread]()
				{
					int kb = lua_gc(main_thread.get(), LUA_GCCOUNT, 0);
					int bytes = lua_gc(main_thread.get(), LUA_GCCOUNTB, 0);
					return static_cast<lua_Number>(kb) * 1024 + static_cast<lua_Number>(bytes);
				});
			});
			module.assert_top();
			return module;
		}
		else if (name == "time" && version == "1.0")
		{
			lua::stack_value module = lua::create_table(*stack.state());
			set_element(
				module,
				"sleep",
				[main_thread, &stack, &io](lua_State &)
			{
				return lua::register_async_function(main_thread, stack, [&io](lua_Number duration_seconds)
				{
					std::chrono::microseconds const duration = lua_duration_to_cpp(duration_seconds);
					auto timer = Si::asio::make_timer(io);
					timer.expires_from_now(duration);
					return Si::transform(
						std::move(timer),
#ifdef _MSC_VER
						std::function<lua::nil(Si::asio::timer_elapsed)> //workaround for lambda-to-funcptr issues
#endif
						([](Si::asio::timer_elapsed)
					{
						return lua::nil();
					}));
				});
			});
			module.assert_top();
			return module;
		}
		else if (name == "async" && version == "1.0")
		{
			lua::stack_value module = lua::create_table(*stack.state());
			set_element(
				module,
				"await_one",
				[main_thread, &stack, &io](lua_State &)
			{
				return lua::register_async_function(main_thread, stack, [main_thread](lua::any_local const &observable, lua_State &stack)
				{
					return lua::observable_into_lua<lua::any_local>(stack, lua::create_reference(main_thread, observable));
				});
			});
			set_element(
				module,
				"constant",
				[main_thread, &stack](lua_State &)
			{
				return lua::register_any_function(stack, [main_thread](lua::any_local const &value, lua_State &stack)
				{
					auto bound_value = Si::to_shared(lua::create_reference(main_thread, value));
					return lua::create_observable(
						stack,
						main_thread,
						Si::make_generator_observable([bound_value]() -> lua::reference const &
					{
						return *bound_value;
					}));
				});
			});
			module.assert_top();
			return module;
		}
		return lua::push_nil(*stack.state());
	}

	struct options
	{
		std::string program;
	};

	boost::optional<options> parse_options(int argc, char **argv)
	{
		options parsed;

		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
		    ("help", "produce help message")
			("program", boost::program_options::value(&parsed.program), "the Lua code file to execute")
		;

		boost::program_options::positional_options_description positional;
		positional.add("program", 1);
		boost::program_options::variables_map vm;
		try
		{
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
		}
		catch (boost::program_options::error const &ex)
		{
			std::cerr
				<< ex.what() << '\n'
				<< desc << "\n";
			return boost::none;
		}

		boost::program_options::notify(vm);

		if (vm.count("help"))
		{
		    std::cerr << desc << "\n";
		    return boost::none;
		}

		return parsed;
	}
}

int main(int argc, char **argv)
{
	boost::optional<options> parsed_options = parse_options(argc, argv);
	if (!parsed_options)
	{
		return 1;
	}

	boost::asio::io_service io;

	auto state = lua::create_lua();
	lua_atpanic(state.get(), [](lua_State *L) -> int
	{
		char const *message = lua_tostring(L, -1);
		std::cerr << message << '\n';
		std::terminate();
	});
	luaopen_base(state.get());
	luaopen_string(state.get());

	lua::main_thread main_thread(*state);
	lua::coroutine runner = lua::create_coroutine(main_thread);
	lua::stack runner_stack(runner.thread());
	lua::result first_level = lua::load_file(runner.thread(), parsed_options->program);
	if (first_level.is_error())
	{
		std::cerr << first_level.code() << ": " << to_string(first_level.get_error()) << '\n';
		return 1;
	}

	try
	{
		{
			lua::stack_value second_level = runner_stack.call(first_level.value(), lua::no_arguments(), std::integral_constant<int, 1>());
			lua::stack::resume_result resumed = runner_stack.resume(
				lua::xmove(std::move(second_level), runner.thread()),
				Si::make_oneshot_generator_source([main_thread, &runner_stack, &io]()
			{
				return lua::register_any_function(runner_stack, [main_thread, &runner_stack, &io](Si::noexcept_string const &name, Si::noexcept_string const &version)
				{
					return require_package(main_thread, runner_stack, io, name, version);
				});
			}));
			assert(Si::try_get_ptr<lua::stack::yield>(resumed));
		}
		io.run();
	}
	catch (std::exception const &ex)
	{
		std::cerr << ex.what() << '\n';
		return 1;
	}
}
