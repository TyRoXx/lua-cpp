#ifndef LUACPP_OBSERVABLE_FROM_LUA_HPP
#define LUACPP_OBSERVABLE_FROM_LUA_HPP

#include <silicium/observable/observer.hpp>
#include <silicium/source/generator_source.hpp>
#include "luacpp/reference.hpp"
#include "luacpp/meta_table.hpp"
#include "luacpp/pcall.hpp"

namespace lua
{
	template <class Observable>
	struct observable_from_lua : Si::observer<typename Observable::element_type>
	{
		explicit observable_from_lua(main_thread main_thread, Observable observable)
			: m_observable(std::move(observable))
			, m_main_thread(main_thread)
		{
		}

		bool async_get_one(any_local const &callback)
		{
			if (!m_callback.empty())
			{
				return false;
			}
			m_callback = create_reference(m_main_thread, callback);
			m_observable.async_get_one(Si::observe_by_ref(static_cast<Si::observer<typename Observable::element_type> &>(*this)));
			return true;
		}

	private:

		Observable m_observable;
		main_thread m_main_thread;
		reference m_callback;

		virtual void got_element(typename Observable::element_type element) SILICIUM_OVERRIDE
		{
			assert(!m_callback.empty());
			stack s(*m_main_thread.get());
			auto callback = std::move(m_callback);
			assert(m_callback.empty());
			variadic_pcall(*s.state(), std::move(callback), std::move(element));
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			assert(!m_callback.empty());
			lua::stack s(*m_main_thread.get());
			auto callback = std::move(m_callback);
			assert(m_callback.empty());
			s.call(
				callback,
				Si::make_oneshot_generator_source([]()
			{
				return nil();
			}),
				0);
		}
	};

	template <class Observable>
	stack_value create_observable_meta_table(lua_State &stack)
	{
		lua::stack s(stack);
		typedef observable_from_lua<Observable> wrapper;
		auto meta = lua::create_default_meta_table<wrapper>(s);
		add_method(s, meta, "async_get_one", &wrapper::async_get_one);
		return meta;
	}

	template <class Observable>
	stack_value create_observable(lua_State &stack, main_thread main_thread, Observable &&observable)
	{
		typedef typename std::decay<Observable>::type clean_observable;
		lua::stack s(stack);
		auto wrapper = emplace_object<observable_from_lua<clean_observable>>(
			s,
			create_observable_meta_table<clean_observable>(stack),
			main_thread,
			std::forward<Observable>(observable)
			);
		return wrapper;
	}
}

#endif
