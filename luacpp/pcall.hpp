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

	inline void pcall(lua_State &L, int arguments, boost::optional<int> expected_results)
	{
		int rc = lua_pcall(&L, arguments, expected_results ? *expected_results : LUA_MULTRET, 0);
		handle_pcall_result(L, rc);
	}

	template <class Function, class ...Arguments>
	void variadic_pcall(lua_State &stack, Function &&function, Arguments &&...arguments)
	{
		using lua::push;
		push(stack, std::forward<Function>(function));
		Si::nothing dummy[] =
		{
			[&]() { push(stack, arguments); return Si::nothing(); }()...
		};
		pcall(stack, static_cast<int>(sizeof...(Arguments)), 1);
	}
}

#endif
