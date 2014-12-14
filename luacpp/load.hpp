#ifndef LUACPP_LOAD_HPP
#define LUACPP_LOAD_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/error.hpp"
#include "luacpp/exception.hpp"
#include "luacpp/path.hpp"
#include <silicium/config.hpp>

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

#if SILICIUM_COMPILER_GENERATES_MOVES
		result(result &&other) BOOST_NOEXCEPT = default;
		result &operator = (result &&other) BOOST_NOEXCEPT = default;
#else
		result(result &&other) BOOST_NOEXCEPT
			: m_rc(other.m_rc)
			, m_error_or_value(std::move(other.m_error_or_value))
		{
			other.m_rc = 0;
		}

		result &operator = (result &&other) BOOST_NOEXCEPT
		{
			using boost::swap;
			swap(m_rc, other.m_rc);
			m_error_or_value = std::move(other.m_error_or_value);
			return *this;
		}
#endif

		int code() const BOOST_NOEXCEPT
		{
			return m_rc;
		}

		bool is_error() const BOOST_NOEXCEPT
		{
			return m_rc != 0;
		}

		any_local const &get_error() const BOOST_NOEXCEPT
		{
			assert(is_error());
			return m_error_or_value;
		}

		stack_value const &value() const
#if SILICIUM_COMPILER_HAS_RVALUE_THIS_QUALIFIER
			&
#endif
		{
			throw_if_error();
			return m_error_or_value;
		}

#if SILICIUM_COMPILER_HAS_RVALUE_THIS_QUALIFIER
		stack_value &value() &
		{
			throw_if_error();
			return m_error_or_value;
		}
#endif

		stack_value &&value()
#if SILICIUM_COMPILER_HAS_RVALUE_THIS_QUALIFIER
			&&
#endif
		{
			throw_if_error();
			return std::move(m_error_or_value);
		}

		void throw_if_error() const
		{
			if (m_rc)
			{
				char const *message = lua_tostring(m_error_or_value.thread(), m_error_or_value.from_bottom());
				assert(message);
				boost::throw_exception(lua_exception(m_rc, message));
			}
		}

	private:

		int m_rc;
		stack_value m_error_or_value;

		SILICIUM_DELETED_FUNCTION(result(result const &))
		SILICIUM_DELETED_FUNCTION(result &operator = (result const &))
	};

	inline result load_buffer(lua_State &stack, Si::memory_range code, char const *name)
	{
		int const rc = luaL_loadbuffer(&stack, code.begin(), code.size(), name);
		stack_value value(stack, lua_gettop(&stack));
		return result(rc, std::move(value));
	}

	inline result load_file(lua_State &stack, boost::filesystem::path const &file)
	{
		int const rc = luaL_loadfile(&stack, to_utf8(file).c_str());
		stack_value value(stack, lua_gettop(&stack));
		return result(rc, std::move(value));
	}
}

#endif
