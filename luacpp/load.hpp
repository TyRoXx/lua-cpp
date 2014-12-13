#ifndef LUACPP_LOAD_HPP
#define LUACPP_LOAD_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/error.hpp"
#include "luacpp/exception.hpp"

namespace lua
{
	struct result
	{
		result() BOOST_NOEXCEPT
			: m_rc(0)
		{
		}

		result(int rc, stack_value error_or_value) BOOST_NOEXCEPT
			: m_rc(rc)
			, m_error_or_value(std::move(error_or_value))
		{
		}

		stack_value &&value() &&
		{
			if (m_rc)
			{
				char const *message = lua_tostring(m_error_or_value.thread(), m_error_or_value.from_bottom());
				assert(message);
				boost::throw_exception(lua_exception(m_rc, message));
			}
			return std::move(m_error_or_value);
		}

	private:

		int m_rc;
		stack_value m_error_or_value;
	};

	inline result load_buffer(lua_State &stack, Si::memory_range code, char const *name)
	{
		int const rc = luaL_loadbuffer(&stack, code.begin(), code.size(), name);
		stack_value value(stack, lua_gettop(&stack));
		return result(rc, std::move(value));
	}
}

#endif
