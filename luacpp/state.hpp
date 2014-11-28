#ifndef LUACPP_STATE_HPP
#define LUACPP_STATE_HPP

#include "luacpp/error.hpp"
#include <memory>
#include <silicium/config.hpp>
#include <silicium/memory_range.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem/path.hpp>

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

	inline boost::system::error_code load_buffer(lua_State &L, Si::memory_range code, char const *name)
	{
		int rc = luaL_loadbuffer(&L, code.begin(), code.size(), name);
		if (rc != 0)
		{
			return boost::system::error_code(rc, get_lua_error_category());
		}
		return boost::system::error_code();
	}

	inline boost::system::error_code load_file(lua_State &L, boost::filesystem::path const &file)
	{
		int rc = luaL_loadfile(&L, file.c_str());
		if (rc != 0)
		{
			return boost::system::error_code(rc, get_lua_error_category());
		}
		return boost::system::error_code();
	}

}

#endif
