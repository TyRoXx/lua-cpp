#include "luacpp/register_any_function.hpp"
#include <silicium/asio/accepting_source.hpp>
#include <silicium/source/generator_source.hpp>
#include <boost/program_options.hpp>
#include <iostream>

namespace
{
	lua::stack_value require_package(lua::stack &stack, Si::noexcept_string const &name, Si::noexcept_string const &version)
	{
		if (name == "web")
		{
			boost::ignore_unused_variable_warning(version);
			lua::stack_value module = stack.create_table();
			stack.set_element(
				lua::any_local(module.from_bottom()),
				"create_server",
				[&stack](lua_State &)
				{
					return lua::register_any_function(stack, [](lua_Integer port, lua::reference on_request)
					{

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

	lua::stack stack(lua::create_lua());
	std::pair<lua::error, lua::stack_value> first_level = stack.load_file(program);
	if (first_level.first != lua::error::success)
	{
		std::cerr << stack.to_string(lua::any_local(first_level.second.from_bottom())) << '\n';
		return 1;
	}

	try
	{
		lua::stack_value second_level = stack.call(first_level.second, lua::no_arguments(), std::integral_constant<int, 1>());
		stack.call(second_level, Si::make_oneshot_generator_source([&stack]()
		{
			return lua::register_any_function(stack, [&stack](Si::noexcept_string const &name, Si::noexcept_string const &version)
			{
				return require_package(stack, name, version);
			});
		}), 0);
	}
	catch (std::exception const &ex)
	{
		std::cerr << ex.what() << '\n';
		return 1;
	}

	io.run();
}
