#ifndef LUACPP_ERROR_HPP
#define LUACPP_ERROR_HPP

#include "luacpp/lua.hpp"
#include <boost/config.hpp>
#include <boost/system/system_error.hpp>
#include <silicium/config.hpp>

namespace lua
{
	enum class error
	{
		success = 0,
		run = LUA_ERRRUN,
		syntax = LUA_ERRSYNTAX,
		mem = LUA_ERRMEM,
		err = LUA_ERRERR,

		///extra error code returned by luaL_loadfile
		file = LUA_ERRFILE
	};

	struct lua_error_category : boost::system::error_category
	{
		virtual const char *name() const BOOST_SYSTEM_NOEXCEPT SILICIUM_OVERRIDE;
		virtual std::string message(int ev) const SILICIUM_OVERRIDE;
	};

	inline const char *lua_error_category::name() const BOOST_SYSTEM_NOEXCEPT
	{
		return "lua";
	}

	inline std::string lua_error_category::message(int ev) const
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

	inline boost::system::error_category const &get_lua_error_category()
	{
		static lua_error_category const instance;
		return instance;
	}
}

#endif
