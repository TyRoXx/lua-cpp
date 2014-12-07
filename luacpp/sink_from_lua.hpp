#ifndef LUACPP_SINK_FROM_LUA_HPP
#define LUACPP_SINK_FROM_LUA_HPP

#include <silicium/sink/sink.hpp>
#include "luacpp/stack.hpp"

namespace lua
{
	template <class Sink>
	struct sink_from_lua
	{
		explicit sink_from_lua(Sink original)
			: m_original(std::move(original))
		{
		}

		void append(lua::any_local elements, lua_State &L)
		{
			typedef typename Sink::element_type element_type;
			auto element = lua::from_lua_cast<element_type>(L, elements.from_bottom());
			Si::success success_expected = m_original.append(Si::make_iterator_range(&element, &element + 1));
			boost::ignore_unused_variable_warning(success_expected);
		}

	private:

		Sink m_original;
	};

	template <class Sink>
	lua::stack_value create_sink_wrapper_meta_table(lua::stack &stack)
	{
		typedef sink_from_lua<Sink> wrapper;
		lua::stack_value table = lua::create_default_meta_table<wrapper>(stack);
		lua::add_method(stack, table, "append", &wrapper::append);
		return table;
	}
}

#endif
