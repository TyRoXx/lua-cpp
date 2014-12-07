#include "luacpp/register_any_function.hpp"
#include "luacpp/meta_table.hpp"
#include "luacpp/sink_into_lua.hpp"
#include "luacpp/pcall.hpp"
#include "luacpp/sink_from_lua.hpp"
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

	struct http_response_generator
	{
		explicit http_response_generator(lua::reference sink)
			: m_sink(std::move(sink))
		{
		}

		void status_line(Si::memory_range status, Si::memory_range status_text, Si::memory_range version, lua_State &state)
		{
			lua::sink_into_lua<char> native_sink(m_sink, state);
			Si::http::generate_status_line(native_sink, version, status, status_text);
		}

		void header(Si::memory_range key, Si::memory_range value, lua_State &state)
		{
			lua::sink_into_lua<char> native_sink(m_sink, state);
			Si::http::generate_header(native_sink, key, value);
		}

		void content(Si::memory_range content, lua_State &state)
		{
			lua::sink_into_lua<char> native_sink(m_sink, state);
			Si::append(native_sink, "\r\n");
			native_sink.append(content);
		}

	private:

		lua::reference m_sink;
	};

	lua::stack_value require_package(
		lua::stack &stack,
		boost::asio::io_service &io,
		Si::noexcept_string const &name,
		Si::noexcept_string const &version)
	{
		if (name == "tcp")
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
					lua::stack_value generator = lua::emplace_object<http_response_generator>(stack, meta, lua::create_reference(*stack.state(), sink));
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
