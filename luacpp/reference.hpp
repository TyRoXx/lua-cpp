#ifndef LUACPP_REFERENCE_HPP
#define LUACPP_REFERENCE_HPP

#include "luacpp/push.hpp"
#include "luacpp/stack_value.hpp"
#include <silicium/config.hpp>

namespace lua
{
	struct reference : pushable
	{
		reference() BOOST_NOEXCEPT
			: m_state(nullptr)
		{
		}

		explicit reference(lua_State &state, int key) BOOST_NOEXCEPT
			: m_state(&state)
			, m_key(key)
		{
		}

		reference(reference &&other) BOOST_NOEXCEPT
			: m_state(other.m_state)
			, m_key(other.m_key)
		{
			other.m_state = nullptr;
		}

		reference &operator = (reference &&other) BOOST_NOEXCEPT
		{
			std::swap(m_state, other.m_state);
			std::swap(m_key, other.m_key);
			return *this;
		}

		~reference() BOOST_NOEXCEPT
		{
			if (!m_state)
			{
				return;
			}
			luaL_unref(m_state, LUA_REGISTRYINDEX, m_key);
		}

		bool empty() const BOOST_NOEXCEPT
		{
			return !m_state;
		}

		void push() const BOOST_NOEXCEPT
		{
			assert(!empty());
			lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_key);
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_state;
		}

		stack_value to_stack_value() const
		{
			push();
			return stack_value(*m_state, lua_gettop(m_state));
		}

		stack_value to_stack_value(lua_State &destination) const
		{
			push(destination);
			return stack_value(destination, lua_gettop(&destination));
		}

		virtual void push(lua_State &L) const SILICIUM_OVERRIDE
		{
			lua_rawgeti(&L, LUA_REGISTRYINDEX, m_key);
		}

	private:

		lua_State *m_state;
		int m_key;

		SILICIUM_DELETED_FUNCTION(reference(reference const &))
		SILICIUM_DELETED_FUNCTION(reference &operator = (reference const &))
	};

	inline void push(lua_State &, reference const &ref) BOOST_NOEXCEPT
	{
		ref.push();
	}

	template <class Pushable>
	reference create_reference(lua_State &L, Pushable const &value)
	{
		push(L, value);
		int key = luaL_ref(&L, LUA_REGISTRYINDEX);
		return reference(L, key);
	}
}

#endif
