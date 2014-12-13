#ifndef LUACPP_STATE_HPP
#define LUACPP_STATE_HPP

#include "luacpp/error.hpp"
#include <memory>
#include <silicium/config.hpp>
#include <silicium/memory_range.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem/path.hpp>
#include <iostream>

namespace lua
{
	struct lua_deleter
	{
		void operator()(lua_State *L) const BOOST_NOEXCEPT;
	};

	inline void lua_deleter::operator()(lua_State *L) const BOOST_NOEXCEPT
	{
		if (!L)
		{
			return;
		}
		lua_close(L);
	}

	typedef std::unique_ptr<lua_State, lua_deleter> state_ptr;

	state_ptr create_lua();

	inline state_ptr create_lua()
	{
		state_ptr lua(luaL_newstate());
		if (!lua)
		{
			throw std::bad_alloc();
		}
		return lua;
	}

	inline void print_stack(std::ostream &out, lua_State &L)
	{
		int size = lua_gettop(&L);
		out << "Size: " << size << '\n';
		for (int i = 1; i <= size; ++i)
		{
			out << "  " << i << ": " << lua_typename(&L, lua_type(&L, i)) << ' ';
			switch (lua_type(&L, i))
			{
			case LUA_TFUNCTION:
			case LUA_TUSERDATA:
			case LUA_TLIGHTUSERDATA:
				out << lua_topointer(&L, i);
				break;
			}
			out << '\n';
		}
	}
}

#endif
