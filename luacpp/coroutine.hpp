#ifndef LUACPP_COROUTINE_HPP
#define LUACPP_COROUTINE_HPP

#include "luacpp/stack.hpp"
#include "luacpp/reference.hpp"

namespace lua
{
	struct coroutine
	{
		explicit coroutine(lua_State &thread)
			: m_thread(&thread)
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
			lua_yield(m_thread, 0);
		}

		void resume()
		{
			lua_resume(m_thread, 0);
		}

	private:

		lua_State *m_thread;
		reference m_life;
	};

	inline coroutine create_coroutine(lua_State &main_thread)
	{
		lua_State * const thread = lua_newthread(&main_thread);
		stack_value discarded(main_thread, lua_gettop(&main_thread));
		return coroutine(*thread);
	}

	inline stack_value xmove(stack_value from, lua_State &to)
	{
		lua_xmove(from.state(), &to, 1);
		from.release();
		return stack_value(to, lua_gettop(&to));
	}
}

#endif
