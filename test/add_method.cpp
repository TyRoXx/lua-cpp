#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/meta_table.hpp"

namespace
{
	struct test_struct
	{
		bool *called;

		explicit test_struct(bool &called)
			: called(&called)
		{
		}

		void method_non_const()
		{
			method_const();
		}

		void method_const() const
		{
			BOOST_CHECK(called);
			BOOST_REQUIRE(!*called);
			*called = true;
		}
	};

	void test_method_call(std::function<void (lua::stack_value &meta)> const &prepare_method)
	{
		test::test_with_environment([&prepare_method](lua::stack &s, test::resource bound)
		{
			lua::stack_value meta = lua::create_default_meta_table<test_struct>(s);
			prepare_method(meta);

			bool called = false;
			lua::stack_value object = lua::emplace_object<test_struct>(s, meta, called);

			lua_getfield(s.state(), -1, "method");

			lua::push(*s.state(), object);

			BOOST_REQUIRE(!called);
			lua::pcall(*s.state(), 1, boost::none);
			BOOST_CHECK(called);
		});
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_from_method_ptr_const)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", &test_struct::method_const);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_from_method_ptr_non_const)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", &test_struct::method_non_const);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_stateless_lambda)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", [](test_struct &instance)
		{
			instance.method_non_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_stateful_lambda)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		long dummy_state = 2;
		lua::add_method(s, meta, "method", [dummy_state](test_struct &instance)
		{
			BOOST_CHECK_EQUAL(2, dummy_state);
			instance.method_non_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_mutable_lambda)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		long dummy_state = 2;
		lua::add_method(s, meta, "method", [dummy_state](test_struct &instance) mutable
		{
			BOOST_CHECK_EQUAL(2, dummy_state);
			instance.method_non_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_this_by_reference)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", [](test_struct &instance)
		{
			instance.method_non_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_const_this_by_reference)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", [](test_struct const &instance)
		{
			instance.method_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_this_by_pointer)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", [](test_struct *instance)
		{
			BOOST_REQUIRE(instance);
			instance->method_non_const();
		});
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_add_method_const_this_by_pointer)
{
	test_method_call([](lua::stack_value &meta)
	{
		lua::stack s(*meta.thread());
		lua::add_method(s, meta, "method", [](test_struct const *instance)
		{
			BOOST_REQUIRE(instance);
			instance->method_const();
		});
	});
}
