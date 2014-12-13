#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/coroutine.hpp"
#include "luacpp/register_any_function.hpp"
#include "luacpp/meta_table.hpp"
#include "luacpp/load.hpp"

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_yield)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value entry_point = coro_stack.register_function([](lua_State *L) -> int
		{
			return lua_yield(L, 0);
		});
		lua::stack::resume_result result = coro_stack.resume(std::move(entry_point), lua::no_arguments());
		BOOST_CHECK(nullptr != Si::try_get_ptr<lua::stack::yield>(result));
	});
}

namespace
{
	struct yielder
	{
		lua::main_thread main_thread;

		explicit yielder(lua::main_thread main_thread)
			: main_thread(main_thread)
		{
		}

		void operator()(lua::current_thread thread)
		{
			return yield(thread);
		}

		void yield(lua::current_thread thread)
		{
			assert(main_thread.get());
			Si::optional<lua::coroutine> coro = lua::pin_coroutine(main_thread, thread);
			BOOST_REQUIRE(coro);
			coro->suspend();
		}
	};
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_yielding_method)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value meta = lua::create_default_meta_table<yielder>(coro_stack);
		lua::add_method(coro_stack, meta, "__call", &yielder::operator ());
		lua::stack_value object = lua::emplace_object<yielder>(coro_stack, meta, lua::main_thread(*s.state()));
		lua::replace(object, meta);
		lua::stack::resume_result result = coro_stack.resume(std::move(object), lua::no_arguments());
		BOOST_CHECK(nullptr != Si::try_get_ptr<lua::stack::yield>(result));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_lua_calls_yielding_method)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());

		lua::stack_value entry_point = lua::load_buffer(coro.thread(), Si::make_c_str_range("return function (yielder) yielder:yield() end"), "test").value();
		BOOST_CHECK_EQUAL(0, lua_status(&coro.thread()));

		lua::stack_value entry_point_2 = coro_stack.call(entry_point, lua::no_arguments(), std::integral_constant<int, 1>());
		BOOST_CHECK_EQUAL(0, lua_status(&coro.thread()));

		lua::replace(entry_point_2, entry_point);
		BOOST_REQUIRE_EQUAL(lua::type::function, coro_stack.get_type(entry_point_2));

		lua::stack_value meta = lua::create_default_meta_table<yielder>(coro_stack);
		lua::add_method(coro_stack, meta, "yield", &yielder::yield);
		lua::stack_value object = lua::emplace_object<yielder>(coro_stack, meta, lua::main_thread(*s.state()));
		lua::replace(object, meta);
		BOOST_REQUIRE_EQUAL(lua::type::user_data, coro_stack.get_type(object));

		entry_point_2.release();
		object.release();
		coro.resume(1);
		BOOST_CHECK_EQUAL(LUA_YIELD, lua_status(&coro.thread()));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_finish)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value entry_point = coro_stack.register_function([](lua_State *L) -> int
		{
			lua_pushinteger(L, 23);
			return 1;
		});
		lua::stack::resume_result result = coro_stack.resume(std::move(entry_point), lua::no_arguments());
		lua::stack_array * const return_values = Si::try_get_ptr<lua::stack_array>(result);
		BOOST_REQUIRE(return_values);
		BOOST_REQUIRE_EQUAL(1, return_values->size());
		BOOST_CHECK_EQUAL(static_cast<lua_Integer>(23), coro_stack.get_integer(*return_values));
	});
}
