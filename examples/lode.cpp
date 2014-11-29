#include "luacpp/register_any_function.hpp"
#include "luacpp/meta_table.hpp"
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/observable/end.hpp>
#include <silicium/source/generator_source.hpp>
#include <boost/program_options.hpp>
#include <iostream>

namespace
{
	struct server : private Si::observer<Si::ended>
	{
		explicit server(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint)
			: m_acceptor(io, endpoint)
			, m_incoming_clients(Si::asio::tcp_acceptor(m_acceptor))
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
		Si::end_observable<Si::asio::tcp_acceptor> m_incoming_clients;
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
						return lua::emplace_object<::server>(stack, [&stack](lua_State &)
						{
							lua::stack_value meta = lua::create_default_meta_table<::server>(stack);
							add_method(stack, meta, "wait", &server::wait);
							add_method(stack, meta, "connection_count", &server::connection_count);
							return meta;
						}, io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), static_cast<boost::uint16_t>(port)));
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
	}
	catch (std::exception const &ex)
	{
		std::cerr << ex.what() << '\n';
		return 1;
	}

	io.run();
}
