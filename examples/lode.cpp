#include "luacpp/register_any_function.hpp"
#include "luacpp/meta_table.hpp"
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/observable/end.hpp>
#include <silicium/observable/transform.hpp>
#include <silicium/observable/ptr.hpp>
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
			coro.resume();

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

	struct server : private Si::observer<Si::ended>
	{
		explicit server(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint, lua::reference on_request)
			: m_acceptor(io, endpoint)
			, m_incoming_clients(
				Si::erase_unique(
					Si::transform(
						Si::asio::tcp_acceptor(m_acceptor),
						[this](Si::asio::tcp_acceptor_result client) -> Si::nothing
						{
							handle_client(std::move(client));
							return {};
						}
					)
				)
			)
			, m_on_request(std::move(on_request))
		{
		}

		void wait(lua::coroutine coro)
		{
			std::cerr << "waiting\n";
			m_waiting = true;
			m_incoming_clients.async_get_one(static_cast<Si::observer<Si::ended> &>(*this));
			if (m_waiting)
			{
				m_suspended = std::move(coro);
				return m_suspended->suspend();
			}
		}

		lua_Integer connection_count() const
		{
			return 0;
		}

	private:

		boost::asio::ip::tcp::acceptor m_acceptor;
		Si::end_observable<Si::unique_observable<Si::nothing>> m_incoming_clients;
		lua::reference m_on_request;
		bool m_waiting;
		boost::optional<lua::coroutine> m_suspended;

		virtual void got_element(Si::ended) SILICIUM_OVERRIDE
		{
			assert(m_waiting);
			m_waiting = false;
			if (m_suspended)
			{
				return Si::exchange(m_suspended, boost::none)->resume();
			}
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
			lua_State &parent = *m_on_request.state();
			lua::stack parent_stack(parent);
			lua::stack_value meta = lua::create_default_meta_table<response_writer>(parent_stack);
			add_method(parent_stack, meta, "add_header", &response_writer::add_header);
			add_method(parent_stack, meta, "set_content", &response_writer::set_content);
			auto response = lua::emplace_object<response_writer>(parent_stack, meta, client.get());

			lua::coroutine child_coro = lua::create_coroutine(parent);
			lua::stack child_stack(child_coro.thread());
			std::array<Si::fast_variant<lua::nil, lua::xmover>, 2> arguments
			{{
				lua::nil(),
				lua::xmover{&response}
			}};
			child_stack.resume(m_on_request.to_stack_value(child_coro.thread()), Si::make_container_source(std::move(arguments)));
			assert(lua_gettop(child_stack.state()) == 0);
		}
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
					return register_any_function(stack, [&stack, &io](lua_Integer port, lua::reference on_request)
					{
						boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), static_cast<boost::uint16_t>(port));
						return lua::emplace_object< ::server>(stack, [&stack](lua_State &)
						{
							lua::stack_value meta = lua::create_default_meta_table< ::server>(stack);
							add_method(stack, meta, "wait", &server::wait);
							add_method(stack, meta, "connection_count", &server::connection_count);
							return meta;
						}, io, endpoint, std::move(on_request));
					});
				}
			);
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
