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

		explicit coroutine(reference thread, bool *suspend_requested)
			: m_life(std::move(thread))
			, m_suspend_requested(suspend_requested)
		{
			thread.push(*m_life.state());
			m_thread = lua_tothread(m_life.state(), -1);
			assert(m_thread);
			lua_pop(m_life.state(), 1);
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
			assert(lua_status(m_thread) == 0);
			assert(m_thread);
			*m_suspend_requested = true;
		}

		void resume(int argument_count)
		{
			assert(lua_status(m_thread) == LUA_YIELD || lua_status(m_thread) == 0);
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

	inline coroutine create_coroutine(main_thread thread)
	{
		assert(thread.get());
		lua_newthread(thread.get());
		stack_value thread_on_stack(*thread.get(), lua_gettop(thread.get()));
		return coroutine(create_reference(thread, thread_on_stack), nullptr);
	}

	inline stack_value xmove(stack_value from, lua_State &to)
	{
		lua_xmove(from.thread(), &to, 1);
		from.release();
		return stack_value(to, lua_gettop(&to));
	}
}

#endif
