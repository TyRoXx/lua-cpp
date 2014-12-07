#ifndef LUACPP_REFERENCE_HPP
#define LUACPP_REFERENCE_HPP

#include "luacpp/push.hpp"
#include "luacpp/stack_value.hpp"
#include <silicium/config.hpp>

namespace lua
{
	inline bool is_main_thread(lua_State &state)
	{
		int rc = lua_pushthread(&state);
		stack_value thread_on_stack(state, lua_gettop(&state));
		return rc == 1;
	}

	struct main_thread
	{
		main_thread()
			: m_state(nullptr)
		{
		}

		explicit main_thread(lua_State &state)
			: m_state(&state)
		{
			assert(is_main_thread(state));
		}

		lua_State *get() const BOOST_NOEXCEPT
		{
			return m_state;
		}

	private:

		lua_State *m_state;
	};

	struct reference : pushable
	{
		reference() BOOST_NOEXCEPT
		{
		}

		explicit reference(main_thread thread, int key) BOOST_NOEXCEPT
			: m_thread(thread)
			, m_key(key)
		{
			assert((lua_pushthread(state()) == 1 && [this]() { lua_pop(state(), 1); return true; }()));
		}

		reference(reference &&other) BOOST_NOEXCEPT
			: m_thread(other.m_thread)
			, m_key(other.m_key)
		{
			other.m_thread = main_thread();
		}

		reference &operator = (reference &&other) BOOST_NOEXCEPT
		{
			std::swap(m_thread, other.m_thread);
			std::swap(m_key, other.m_key);
			return *this;
		}

		~reference() BOOST_NOEXCEPT
		{
			if (!state())
			{
				return;
			}
			luaL_unref(state(), LUA_REGISTRYINDEX, m_key);
		}

		bool empty() const BOOST_NOEXCEPT
		{
			return !state();
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_thread.get();
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

		type get_type() const
		{
			return to_stack_value(*state()).get_type();
		}

	private:

		main_thread m_thread;
		int m_key;

		SILICIUM_DELETED_FUNCTION(reference(reference const &))
		SILICIUM_DELETED_FUNCTION(reference &operator = (reference const &))
	};

	template <class Pushable>
	reference create_reference(main_thread thread, Pushable const &value)
	{
		assert(thread.get());
		push(*thread.get(), value);
		int key = luaL_ref(thread.get(), LUA_REGISTRYINDEX);
		return reference(thread, key);
	}
}

#endif
