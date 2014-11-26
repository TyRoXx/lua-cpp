#include "lua_environment.hpp"

namespace lua
{
	const char *lua_error_category::name() const BOOST_SYSTEM_NOEXCEPT
	{
		return "lua";
	}

	std::string lua_error_category::message(int ev) const
	{
		switch (ev)
		{
		case LUA_ERRMEM: return "LUA_ERRMEM";
		case LUA_ERRRUN: return "LUA_ERRRUN";
		case LUA_ERRSYNTAX: return "LUA_ERRSYNTAX";
		case LUA_ERRERR: return "LUA_ERRERR";
		}
		return "";
	}

	boost::system::error_category const &get_lua_error_category()
	{
		static lua_error_category const instance;
		return instance;
	}

	void lua_deleter::operator()(lua_State *L) const BOOST_NOEXCEPT
	{
		if (!L)
		{
			return;
		}
		lua_close(L);
	}

	std::unique_ptr<lua_State, lua_deleter> create_lua()
	{
		std::unique_ptr<lua_State, lua_deleter> lua(luaL_newstate());
		if (!lua)
		{
			throw std::bad_alloc();
		}
		return lua;
	}

	boost::system::error_code load_buffer(lua_State &L, Si::memory_range code, char const *name)
	{
		int rc = luaL_loadbuffer(&L, code.begin(), code.size(), name);
		if (rc != 0)
		{
			return boost::system::error_code(rc, get_lua_error_category());
		}
		return boost::system::error_code();
	}

	void push(lua_State &L, stack_value const &value)
	{
		lua_pushvalue(&L, value.from_bottom());
	}
}
