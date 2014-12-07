#include "luacpp/register_any_function.hpp"
#include "luacpp/meta_table.hpp"
#include "luacpp/pcall.hpp"
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/observable/end.hpp>
#include <silicium/observable/transform.hpp>
#include <silicium/observable/ptr.hpp>
#include <silicium/observable/generator.hpp>
#include <silicium/observable/constant.hpp>
#include <silicium/source/memory_source.hpp>
#include <silicium/source/generator_source.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <boost/program_options.hpp>
#include <iostream>

namespace
{
	struct response_writer : private Si::observer<boost::system::error_code>
	{
		explicit response_writer(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
			: m_socket(std::move(socket))
		{
		}

		void add_header(Si::noexcept_string const &key, Si::noexcept_string const &value)
		{
			auto sink = Si::make_container_sink(m_send_buffer);
			if (m_send_buffer.empty())
			{
				Si::http::generate_status_line(sink, "HTTP/1.0", "200", "OK");
			}
			Si::http::generate_header(sink, key, value);
		}

		void set_content(Si::noexcept_string const &content, lua::coroutine coro)
		{
			add_header("Content-Length", boost::lexical_cast<Si::noexcept_string>(content.size()));
			m_send_buffer.push_back('\r');
			m_send_buffer.push_back('\n');
			m_send_buffer.insert(m_send_buffer.end(), content.begin(), content.end());
			return send(std::move(coro));
		}

	private:

		typedef Si::asio::writing_observable<boost::asio::ip::tcp::socket, Si::constant_observable<Si::memory_range>> writer;

		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		std::vector<char> m_send_buffer;
		writer m_writer;
		lua::coroutine m_suspended;

		void send(lua::coroutine coro)
		{
			assert(m_suspended.empty());
			assert(!m_send_buffer.empty());
			m_writer = writer(*m_socket, Si::make_constant_observable(Si::make_memory_range(m_send_buffer)));
			m_writer.async_get_one(static_cast<Si::observer<boost::system::error_code> &>(*this));
			m_suspended = std::move(coro);
			return coro.suspend();
		}

		virtual void got_element(boost::system::error_code value) SILICIUM_OVERRIDE
		{
			assert(!m_suspended.empty());
			auto coro = std::move(m_suspended);
			coro.resume(0);

			//ignore the request for now
			boost::array<char, 1024> buffer;
			boost::system::error_code ec;
			m_socket->read_some(boost::asio::buffer(buffer), ec);

			m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			SILICIUM_UNREACHABLE();
		}
	};

	struct tcp_client : private Si::observer<boost::system::error_code>
	{
		explicit tcp_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
			: m_socket(std::move(socket))
			, m_sender(Si::asio::make_writing_observable(*m_socket, Si::erase_unique(Si::make_generator_observable([this]() -> Si::memory_range
			{
				return Si::make_memory_range(m_send_buffer);
			}))))
		{
			assert(m_socket);
		}

		void append(lua_Integer element)
		{
			m_send_buffer.push_back(static_cast<char>(element));
		}

		void flush(lua::coroutine coro)
		{
			m_sender.async_get_one(*this);
			coro.suspend();
			m_coro = std::move(coro);
		}

	private:

		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		Si::noexcept_string m_send_buffer;
		Si::asio::writing_observable<boost::asio::ip::tcp::socket, Si::unique_observable<Si::memory_range>> m_sender;
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

	struct tcp_acceptor : private Si::observer<Si::asio::tcp_acceptor_result>
	{
		explicit tcp_acceptor(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint)
			: m_acceptor(io, endpoint)
			, m_incoming_clients(m_acceptor)
		{
		}

		void sync_get(lua::coroutine coro)
		{
			m_waiting = true;
			m_incoming_clients.async_get_one(static_cast<Si::observer<Si::asio::tcp_acceptor_result> &>(*this));
			if (m_waiting)
			{
				m_suspended = std::move(coro);
				return m_suspended->suspend();
			}
		}

	private:

		boost::asio::ip::tcp::acceptor m_acceptor;
		Si::asio::tcp_acceptor m_incoming_clients;
		bool m_waiting;
		boost::optional<lua::coroutine> m_suspended;

		virtual void got_element(Si::asio::tcp_acceptor_result incoming) SILICIUM_OVERRIDE
		{
			assert(m_waiting);
			assert(m_suspended);
			m_waiting = false;

			assert(lua_status(&m_suspended->thread()) == LUA_YIELD);

			if (incoming.is_error())
			{
				return Si::exchange(m_suspended, boost::none)->resume(1);
			}

			lua::coroutine child_coro = *std::move(Si::exchange(m_suspended, boost::none));
			lua::stack child_stack(child_coro.thread());

			assert(child_stack.size() == 0);

			lua::stack_value meta = lua::create_default_meta_table<tcp_client>(child_stack);
			assert(child_stack.size() == 1);
			assert(child_stack.get_type(meta) == lua::type::table);

			add_method(child_stack, meta, "append", &tcp_client::append);
			assert(child_stack.size() == 1);
			assert(child_stack.get_type(meta) == lua::type::table);

			lua::add_method(child_stack, meta, "flush", &tcp_client::flush);
			assert(child_stack.size() == 1);
			assert(child_stack.get_type(meta) == lua::type::table);

			lua::stack_value client = lua::emplace_object<tcp_client>(child_stack, meta, incoming.get());
			replace(client, meta);

			assert(lua_gettop(child_stack.state()) == 1);
			client.release();
			child_coro.resume(1);
			assert(lua_gettop(child_stack.state()) == 0);
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			SILICIUM_UNREACHABLE();
		}
	};

	struct server : private Si::observer<Si::asio::tcp_acceptor_result>
	{
		explicit server(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint)
			: m_acceptor(io, endpoint)
			, m_incoming_clients(m_acceptor)
		{
		}

		void sync_get(lua::coroutine coro)
		{
			m_waiting = true;
			m_incoming_clients.async_get_one(static_cast<Si::observer<Si::asio::tcp_acceptor_result> &>(*this));
			if (m_waiting)
			{
				m_suspended = std::move(coro);
				return m_suspended->suspend();
			}
		}

	private:

		boost::asio::ip::tcp::acceptor m_acceptor;
		Si::asio::tcp_acceptor m_incoming_clients;
		bool m_waiting;
		boost::optional<lua::coroutine> m_suspended;

		virtual void got_element(Si::asio::tcp_acceptor_result incoming) SILICIUM_OVERRIDE
		{
			assert(m_waiting);
			assert(m_suspended);
			m_waiting = false;

			if (incoming.is_error())
			{
				return Si::exchange(m_suspended, boost::none)->resume(1);
			}

			lua::coroutine &child_coro = *m_suspended;
			lua::stack child_stack(child_coro.thread());

			lua::stack_value meta = lua::create_default_meta_table<response_writer>(child_stack);
			add_method(child_stack, meta, "add_header", &response_writer::add_header);
			add_method(child_stack, meta, "set_content", &response_writer::set_content);
			lua::stack_value response = lua::emplace_object<response_writer>(child_stack, meta, incoming.get());
			replace(response, meta);

			assert(lua_gettop(child_stack.state()) == 1);
			response.release();
			child_coro.resume(1);
			assert(lua_gettop(child_stack.state()) == 0);
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			SILICIUM_UNREACHABLE();
		}

		void handle_client(Si::asio::tcp_acceptor_result client)
		{
			if (client.is_error())
			{
				return;
			}
		}
	};

	template <class T>
	struct sink_into_lua
	{
		typedef T element_type;
		typedef Si::success error_type;

		explicit sink_into_lua(lua::reference handler)
			: m_handler(std::move(handler))
			, m_state(nullptr)
		{
		}

		void set_state(lua_State &state)
		{
			m_state = &state;
		}

		error_type append(Si::iterator_range<element_type const *> data)
		{
			assert(m_state);
			for (element_type const &element : data)
			{
				lua::push(*m_state, m_handler);
				lua_getfield(m_state, -1, "append");
				lua::push(*m_state, m_handler);
				lua::push(*m_state, element);
				lua::pcall(*m_state, 2, 0);
			}
			return error_type();
		}

	private:

		lua::reference m_handler;
		lua_State *m_state;
	};

	template <class Sink>
	struct sink_from_lua
	{
		explicit sink_from_lua(Sink original)
			: m_original(std::move(original))
		{
		}

		void append(lua::any_local elements, lua_State &L)
		{
			typedef typename Sink::element_type element_type;
			auto element = lua::from_lua_cast<element_type>(L, elements.from_bottom());
			Si::success success_expected = m_original.append(Si::make_iterator_range(&element, &element + 1));
			boost::ignore_unused_variable_warning(success_expected);
		}

	private:

		Sink m_original;
	};

	template <class Sink>
	lua::stack_value create_sink_wrapper_meta_table(lua::stack &stack)
	{
		lua::stack_value table = lua::create_default_meta_table<sink_from_lua<Sink>>(stack);
		lua::add_method(stack, table, "append", &sink_from_lua<Sink>::append);
		return table;
	}

	struct http_response_generator
	{
		explicit http_response_generator(sink_into_lua<char> sink)
			: m_sink(std::move(sink))
		{
		}

		void status_line(Si::memory_range status, Si::memory_range status_text, Si::memory_range version, lua_State &state)
		{
			m_sink.set_state(state);
			Si::http::generate_status_line(m_sink, version, status, status_text);
		}

		void header(Si::memory_range key, Si::memory_range value, lua_State &state)
		{
			m_sink.set_state(state);
			Si::http::generate_header(m_sink, key, value);
		}

		void content(Si::memory_range content, lua_State &state)
		{
			m_sink.set_state(state);
			Si::append(m_sink, "\r\n");
			m_sink.append(content);
		}

	private:

		sink_into_lua<char> m_sink;
	};

	lua::stack_value require_package(
		lua::stack &stack,
		boost::asio::io_service &io,
		Si::noexcept_string const &name,
		Si::noexcept_string const &version)
	{
		if (name == "web")
		{
			boost::ignore_unused_variable_warning(version);
			lua::stack_value module = stack.create_table();
			stack.set_element(
				lua::any_local(module.from_bottom()),
				"create_server",
				[&stack, &io](lua_State &)
				{
					return register_any_function(stack, [&stack, &io](lua_Integer port)
					{
						boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), static_cast<boost::uint16_t>(port));
						return lua::emplace_object< ::server>(stack, [&stack](lua_State &)
						{
							lua::stack_value meta = lua::create_default_meta_table< ::server>(stack);
							add_method(stack, meta, "sync_get", &server::sync_get);
							return meta;
						}, io, endpoint);
					});
				}
			);
			module.assert_top();
			return module;
		}
		else if (name == "tcp")
		{
			lua::stack_value module = stack.create_table();
			stack.set_element(
				module,
				"create_acceptor",
				[&stack, &io](lua_State &)
			{
				return lua::register_any_function(stack, [&stack, &io](lua_Integer port)
				{
					boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), static_cast<boost::uint16_t>(port));
					return lua::emplace_object< ::tcp_acceptor>(stack, [&stack](lua_State &)
					{
						lua::stack_value meta = lua::create_default_meta_table< ::tcp_acceptor>(stack);
						add_method(stack, meta, "sync_get", &tcp_acceptor::sync_get);
						return meta;
					}, io, endpoint);
				});
			});
			module.assert_top();
			return module;
		}
		else if (name == "http")
		{
			lua::stack_value module = stack.create_table();
			stack.set_element(
				module,
				"make_response_generator",
				[&stack, &io](lua_State &)
			{
				return lua::register_any_function(stack, [&stack, &io](lua::any_local const &sink)
				{
					lua::stack_value meta = lua::create_default_meta_table<http_response_generator>(stack);
					lua::add_method(stack, meta, "status_line", &http_response_generator::status_line);
					lua::add_method(stack, meta, "header", &http_response_generator::header);
					lua::add_method(stack, meta, "content", &http_response_generator::content);
					lua::stack_value generator = lua::emplace_object<http_response_generator>(stack, meta, sink_into_lua<char>(lua::create_reference(*stack.state(), sink)));
					lua::replace(generator, meta);
					return generator;
				});
			});
			module.assert_top();
			return module;
		}
		return stack.push_nil();
	}
}

int main(int argc, char **argv)
{
	std::string program;

	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
	    ("help", "produce help message")
		("program", boost::program_options::value(&program), "the Lua code file to execute")
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
		return 1;
	}

	boost::program_options::notify(vm);

	if (vm.count("help"))
	{
	    std::cerr << desc << "\n";
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
	lua::stack stack(*state);
	std::pair<lua::error, lua::stack_value> first_level = stack.load_file(program);
	if (first_level.first != lua::error::success)
	{
		std::cerr << stack.to_string(lua::any_local(first_level.second.from_bottom())) << '\n';
		return 1;
	}

	try
	{
		lua::stack_value second_level = stack.call(first_level.second, lua::no_arguments(), std::integral_constant<int, 1>());
		lua::coroutine runner = lua::create_coroutine(*stack.state());
		lua::stack runner_stack(runner.thread());
		lua::stack::resume_result resumed = runner_stack.resume(
			lua::xmove(std::move(second_level), runner.thread()),
			Si::make_oneshot_generator_source([&runner_stack, &io]()
		{
			return lua::register_any_function(runner_stack, [&runner_stack, &io](Si::noexcept_string const &name, Si::noexcept_string const &version)
			{
				return require_package(runner_stack, io, name, version);
			});
		}));
		assert(Si::try_get_ptr<lua::stack::yield>(resumed));
		io.run();
	}
	catch (std::exception const &ex)
	{
		std::cerr << ex.what() << '\n';
		return 1;
	}
}
