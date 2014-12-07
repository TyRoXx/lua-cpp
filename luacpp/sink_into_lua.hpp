#ifndef LUACPP_SINK_INTO_LUA_HPP
#define LUACPP_SINK_INTO_LUA_HPP

#include <silicium/sink/sink.hpp>
#include "luacpp/reference.hpp"

namespace lua
{
	template <class T>
	struct sink_into_lua
	{
		typedef T element_type;
		typedef Si::success error_type;

		explicit sink_into_lua(const lua::reference &handler, lua_State &state)
			: m_handler(handler)
			, m_state(&state)
		{
			assert(handler.get_type() == type::user_data);
		}

		error_type append(Si::iterator_range<element_type const *> data)
		{
			assert(m_state);
			for (element_type const &element : data)
			{
				lua::push(*m_state, m_handler);
				lua_getfield(m_state, -1, "append");
				lua::push(*m_state, m_handler);
				lua::push(*m_state, element);
				lua::pcall(*m_state, 2, 0);
			}
			return error_type();
		}

	private:

		const lua::reference &m_handler;
		lua_State *m_state;
	};
}

#endif
