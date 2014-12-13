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
}

#endif
