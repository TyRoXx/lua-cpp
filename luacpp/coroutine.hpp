#ifndef LUACPP_COROUTINE_HPP
#define LUACPP_COROUTINE_HPP

#include "luacpp/stack.hpp"
#include "luacpp/reference.hpp"

namespace lua
{
	struct coroutine
	{
		coroutine()
			: m_thread(nullptr)
			, m_suspend_requested(nullptr)
		{
		}

		explicit coroutine(lua_State &thread, bool *suspend_requested)
			: m_thread(&thread)
			, m_suspend_requested(suspend_requested)
		{
			int rc = lua_pushthread(&thread);
			assert((rc != 1) && "you cannot construct a coroutine handle from the Lua main thread");
			boost::ignore_unused_variable_warning(rc);
			m_life = create_reference(thread, stack_value(thread, lua_gettop(&thread)));
		}

		lua_State &thread() const
		{
			assert(m_thread);
			return *m_thread;
		}

		void suspend()
		{
			assert(m_suspend_requested);
			assert(!*m_suspend_requested);
			*m_suspend_requested = true;
		}

		void resume(int argument_count)
		{
			int const rc = lua_resume(m_thread, argument_count);
			if (rc && (rc != LUA_YIELD))
			{
				std::string message = lua_tostring(m_thread, -1);
				lua_pop(m_thread, 1);
				boost::throw_exception(lua_exception(std::move(message)));
			}
		}

		bool empty() const BOOST_NOEXCEPT
		{
			return m_thread == nullptr;
		}

	private:

		lua_State *m_thread;
		reference m_life;
		bool *m_suspend_requested;
	};

	inline coroutine create_coroutine(lua_State &main_thread)
	{
		lua_State * const thread = lua_newthread(&main_thread);
		stack_value discarded(main_thread, lua_gettop(&main_thread));
		return coroutine(*thread, nullptr);
	}

	inline stack_value xmove(stack_value from, lua_State &to)
	{
		lua_xmove(from.state(), &to, 1);
		from.release();
		return stack_value(to, lua_gettop(&to));
	}
}

#endif
