#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/observable_into_lua.hpp"
#include "luacpp/load.hpp"
#include <silicium/observable/for_each.hpp>
#include <silicium/observable/ref.hpp>
#include <silicium/observable/consume.hpp>

BOOST_AUTO_TEST_CASE(lua_wrapper_observable_into_lua)
{
	test::test_with_environment([](lua::stack &s, test::resource)
	{
		lua::stack_value program = lua::load_buffer(*s.state(), Si::make_c_str_range(
			"local observable = {}\n"
			"function observable:async_get_one(callback)\n"
			"    callback(\"hallo\")\n"
			"end\n"
			"return observable\n"
			), "test").value();
		BOOST_REQUIRE_EQUAL(lua::type::function, lua::get_type(program));
		lua::stack_value lua_observable = s.call(program, lua::no_arguments(), lua::one());
		BOOST_REQUIRE_EQUAL(lua::type::table, lua::get_type(lua_observable));
		lua::observable_into_lua<Si::noexcept_string> cpp_observable(*s.state(), lua::create_reference(lua::main_thread(*s.state()), lua_observable));
		lua_observable.pop();
		program.pop();

		bool got_one = false;
		auto consumer = Si::consume<Si::noexcept_string>([&got_one](Si::noexcept_string const &element)
		{
			BOOST_REQUIRE(!got_one);
			got_one = true;
			BOOST_CHECK_EQUAL("hallo", element);
		});
		BOOST_CHECK(!got_one);
		cpp_observable.async_get_one(consumer);
		BOOST_CHECK(got_one);
	});
}
