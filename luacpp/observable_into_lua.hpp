#ifndef LUACPP_OBSERVABLE_INTO_LUA_HPP
#define LUACPP_OBSERVABLE_INTO_LUA_HPP

#include <silicium/observable/observer.hpp>
#include <silicium/exchange.hpp>
#include "luacpp/reference.hpp"
#include "luacpp/register_any_function.hpp"

namespace lua
{
	template <class Element>
	struct observable_into_lua
	{
		typedef Element element_type;

		explicit observable_into_lua(lua_State &stack, reference observable)
			: m_state(std::make_shared<async_state>(stack, std::move(observable)))
		{
		}

		void async_get_one(Si::observer<element_type> &observer)
		{
			assert(m_state);
			assert(!m_state->m_observer);
			m_state->m_observer = &observer;
			auto method = get_element(to_local(*m_state->m_stack, m_state->m_observable), "async_get_one");
			auto this_ = to_local(*m_state->m_stack, m_state->m_observable);
			lua::stack s(*m_state->m_stack);
			std::weak_ptr<async_state> state = m_state;
			auto callback = register_any_function(s, [state](any_local const &element)
			{
				auto state_locked = state.lock();
				if (!state_locked)
				{
					//result is obsolete
					return;
				}
				if (get_type(element) == type::nil)
				{
					Si::exchange(state_locked->m_observer, nullptr)->ended();
				}
				else
				{
					Si::exchange(state_locked->m_observer, nullptr)->got_element(from_lua_cast<element_type>(element));
				}
			});
			callback.release();
			this_.release();
			method.release();
			pcall(*m_state->m_stack, 1, boost::none);
		}

	private:

		struct async_state
		{
			lua_State *m_stack;
			reference m_observable;
			Si::observer<element_type> *m_observer;

			async_state(lua_State &stack, reference observable)
				: m_stack(&stack)
				, m_observable(std::move(observable))
				, m_observer(nullptr)
			{
			}
		};

		std::shared_ptr<async_state> m_state;
	};
}

#endif
