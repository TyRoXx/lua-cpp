#include <boost/test/unit_test.hpp>
#include "luacpp/register_async_function.hpp"
#include <silicium/observable/bridge.hpp>
#include <silicium/observable/ref.hpp>

BOOST_AUTO_TEST_CASE(lua_wrapper_register_async_function)
{
	auto state = lua::create_lua();
	lua::main_thread main_thread(*state);
	lua::stack main_stack(*state);
	Si::bridge<lua_Number> observable;
	auto func = lua::register_async_function(main_thread, main_stack, [&observable]()
	{
		return Si::ref(observable);
	});

	lua::coroutine coro = lua::create_coroutine(main_thread);
	lua::stack coro_stack(coro.thread());
	lua::stack::resume_result resumed = coro_stack.resume(std::move(func), lua::no_arguments());
	BOOST_CHECK(nullptr != Si::try_get_ptr<lua::stack::yield>(resumed));
	BOOST_CHECK(observable.is_waiting());
}
