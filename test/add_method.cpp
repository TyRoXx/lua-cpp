#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/meta_table.hpp"

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_trivial)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		struct test_struct
		{
			bool *called;

			explicit test_struct(bool &called)
				: called(&called)
			{
			}

			void method()
			{
				BOOST_CHECK(called);
				BOOST_REQUIRE(!*called);
				*called = true;
			}
		};

		lua::stack_value meta = lua::create_default_meta_table<test_struct>(s);
		lua::add_method(s, meta, "method", &test_struct::method);

		bool called = false;
		lua::stack_value object = lua::emplace_object<test_struct>(s, meta, called);
		
		lua_getfield(s.state(), -1, "method");

		lua::push(*s.state(), object);

		BOOST_REQUIRE(!called);
		lua::pcall(*s.state(), 1, boost::none);
		BOOST_CHECK(called);
	});
}
