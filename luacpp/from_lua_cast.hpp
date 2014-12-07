#ifndef LUACPP_FROM_LUA_CAST_HPP
#define LUACPP_FROM_LUA_CAST_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/reference.hpp"
#include <silicium/memory_range.hpp>

namespace lua
{
	template <class T>
	struct from_lua;

	template <>
	struct from_lua<lua_Number>
	{
		lua_Number operator()(lua_State &L, int address) const
		{
			return lua_tonumber(&L, address);
		}
	};

	template <>
	struct from_lua<lua_Integer>
	{
		lua_Integer operator()(lua_State &L, int address) const
		{
			return lua_tointeger(&L, address);
		}
	};

	template <>
	struct from_lua<bool>
	{
		bool operator()(lua_State &L, int address) const
		{
			return lua_toboolean(&L, address) != 0;
		}
	};

	template <>
	struct from_lua<void *>
	{
		void *operator()(lua_State &L, int address) const
		{
			return lua_touserdata(&L, address);
		}
	};

	template <>
	struct from_lua<Si::noexcept_string>
	{
		Si::noexcept_string operator()(lua_State &L, int address) const
		{
			char const *raw = lua_tostring(&L, address);
			if (!raw)
			{
				return Si::noexcept_string();
			}
			return raw;
		}
	};

	template <>
	struct from_lua<char const *>
	{
		char const *operator()(lua_State &L, int address) const
		{
			return lua_tostring(&L, address);
		}
	};

	template <>
	struct from_lua<Si::memory_range>
	{
		Si::memory_range operator()(lua_State &L, int address) const
		{
			std::size_t length = 0;
			char const *begin = lua_tolstring(&L, address, &length);
			return Si::make_memory_range(begin, begin + length);
		}
	};

	template <class T>
	T from_lua_cast(lua_State &L, int address)
	{
		return from_lua<T>()(L, address);
	}

	template <class T>
	T from_lua_cast(any_local address)
	{
		return from_lua<T>()(*address.thread(), address.from_bottom());
	}

	template <class T>
	T from_lua_cast(lua_State &L, reference const &ref)
	{
		ref.push(L);
		stack_value pushed(L, lua_gettop(&L));
		return from_lua<T>()(L, pushed.from_bottom());
	}
}

#endif
