#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/pcall.hpp"
#include "luacpp/load.hpp"

BOOST_AUTO_TEST_CASE(lua_pcall_0_results)
{
	test::test_with_environment([](lua::stack &s, test::resource)
	{
		lua::load_buffer(*s.state(), Si::make_c_str_range("return nil"), "test").value().release();
		lua::stack_array results = lua::pcall(*s.state(), 0, 0);
		BOOST_CHECK_EQUAL(0, results.size());
	});
}

BOOST_AUTO_TEST_CASE(lua_pcall_1_result)
{
	test::test_with_environment([](lua::stack &s, test::resource)
	{
		lua::load_buffer(*s.state(), Si::make_c_str_range("return 12"), "test").value().release();
		lua::stack_array results = lua::pcall(*s.state(), 0, 1);
		BOOST_REQUIRE_EQUAL(1, results.size());
		BOOST_CHECK_EQUAL(12, lua::to_integer(results.element_address(0)));
	});
}

BOOST_AUTO_TEST_CASE(lua_pcall_2_results)
{
	test::test_with_environment([](lua::stack &s, test::resource)
	{
		lua::load_buffer(*s.state(), Si::make_c_str_range("return 12, 34"), "test").value().release();
		lua::stack_array results = lua::pcall(*s.state(), 0, 2);
		BOOST_REQUIRE_EQUAL(2, results.size());
		BOOST_CHECK_EQUAL(12, lua::to_integer(results.element_address(0)));
		BOOST_CHECK_EQUAL(34, lua::to_integer(results.element_address(1)));
	});
}

BOOST_AUTO_TEST_CASE(lua_pcall_2_arguments)
{
	test::test_with_environment([](lua::stack &s, test::resource)
	{
		auto script = lua::load_buffer(*s.state(), Si::make_c_str_range("return function (a, b) return b + 1, a + 2 end"), "test").value();
		script.release();
		auto function = lua::pcall(*s.state(), 0, 1);
		BOOST_REQUIRE_EQUAL(1, function.size());
		function.release();
		lua::push(*s.state(), 43.0);
		lua::push(*s.state(), 60.0);
		lua::stack_array results = lua::pcall(*s.state(), 2, static_cast<lua_Integer>(2));
		BOOST_REQUIRE_EQUAL(2, results.size());
		BOOST_CHECK_EQUAL(60 + 1, lua::to_integer(results.element_address(0)));
		BOOST_CHECK_EQUAL(43 + 2, lua::to_integer(results.element_address(1)));
	});
}
