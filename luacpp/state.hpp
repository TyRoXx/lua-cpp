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

	inline type get_type(any_local const &local)
	{
		return static_cast<type>(lua_type(local.thread(), local.from_bottom()));
	}

	inline lua_Number to_number(any_local const &local)
	{
		return lua_tonumber(local.thread(), local.from_bottom());
	}

	inline lua_Integer to_integer(any_local const &local)
	{
		return lua_tointeger(local.thread(), local.from_bottom());
	}

	inline Si::noexcept_string to_string(any_local const &local)
	{
		return lua_tostring(local.thread(), local.from_bottom());
	}

	inline bool to_boolean(any_local const &local)
	{
		return lua_toboolean(local.thread(), local.from_bottom()) != 0;
	}

	inline void *to_user_data(any_local const &local)
	{
		return lua_touserdata(local.thread(), local.from_bottom());
	}

	inline boost::optional<lua_Number> get_number(any_local const &local)
	{
		type const t = get_type(local);
		if (t != type::number)
		{
			return boost::none;
		}
		return to_number(local);
	}

	inline boost::optional<lua_Integer> get_integer(any_local const &local)
	{
		type const t = get_type(local);
		if (t != type::number)
		{
			return boost::none;
		}
		return to_integer(local);
	}

	inline boost::optional<Si::noexcept_string> get_string(any_local const &local)
	{
		type const t = get_type(local);
		if (t != type::string)
		{
			return boost::none;
		}
		return to_string(local);
	}

	inline boost::optional<bool> get_boolean(any_local const &local)
	{
		type const t = get_type(local);
		if (t != type::boolean)
		{
			return boost::none;
		}
		return to_boolean(local);
	}
}

#endif
