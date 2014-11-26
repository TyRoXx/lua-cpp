#ifndef LUACPP_TYPE_HPP
#define LUACPP_TYPE_HPP

#include "luacpp/lua.hpp"
#include <ostream>

namespace lua
{
	enum class type
	{
		nil = LUA_TNIL,
		boolean = LUA_TBOOLEAN,
		light_user_data = LUA_TLIGHTUSERDATA,
		number = LUA_TNUMBER,
		string = LUA_TSTRING,
		table = LUA_TTABLE,
		function = LUA_TFUNCTION,
		user_data = LUA_TUSERDATA,
		thread = LUA_TTHREAD
	};

	inline std::ostream &operator << (std::ostream &out, type value)
	{
		switch (value)
		{
		case type::nil: return out << "nil";
		case type::boolean: return out << "boolean";
		case type::light_user_data: return out << "light_user_data";
		case type::number: return out << "number";
		case type::string: return out << "string";
		case type::table: return out << "table";
		case type::function: return out << "function";
		case type::user_data: return out << "user_data";
		case type::thread: return out << "thread";
		}
		return out << "??";
	}
}

#endif
