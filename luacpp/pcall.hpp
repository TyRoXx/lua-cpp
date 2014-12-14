#ifndef LUACPP_PCALL_HPP
#define LUACPP_PCALL_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/exception.hpp"

namespace lua
{
	inline void handle_pcall_result(lua_State &L, int rc)
	{
		if (rc == 0)
		{
			return;
		}
		//TODO: stack trace in case of an error
		std::string message = lua_tostring(&L, -1);
		lua_pop(&L, 1);
		boost::throw_exception(lua_exception(rc, std::move(message)));
	}

	SILICIUM_USE_RESULT
	inline stack_array pcall(lua_State &L, int arguments, boost::optional<int> expected_results)
	{
		int stack_before = lua_gettop(&L);
		int rc = lua_pcall(&L, arguments, expected_results ? *expected_results : LUA_MULTRET, 0);
		handle_pcall_result(L, rc);
		int result_count = lua_gettop(&L) - stack_before - arguments;
		return stack_array(L, size(L) - result_count + 1);
	}

	template <class Function, class ...Arguments>
	void variadic_pcall(lua_State &stack, Function &&function, Arguments &&...arguments)
	{
		using lua::push;
		push(stack, std::forward<Function>(function));

		//Use array initialization to unpack varadic parameters and push the
		//arguments in first-to-last order.
		Si::nothing everything_pushed[] =
		{
			[&]()
			{
				push(stack, arguments);
				return Si::nothing();
			}()...
		};
		boost::ignore_unused_variable_warning(everything_pushed);

		pcall(stack, static_cast<int>(sizeof...(Arguments)), 0);
	}
}

#endif
