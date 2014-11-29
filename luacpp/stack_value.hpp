#ifndef LUACPP_STACK_VALUE_HPP
#define LUACPP_STACK_VALUE_HPP

#include "luacpp/type.hpp"
#include "luacpp/push.hpp"
#include <boost/config.hpp>
#include <silicium/config.hpp>
#include <cassert>

namespace lua
{
	template <class T>
	struct variable
	{
		T value;
	};

	template <class Size>
	struct basic_stack_value : private Size, public any_local
	{
		basic_stack_value() BOOST_NOEXCEPT
			: m_state(nullptr)
		{
		}

		basic_stack_value(lua_State &state, int address, Size size = Size()) BOOST_NOEXCEPT
			: Size(size)
			, any_local(address)
			, m_state(&state)
#ifndef NDEBUG
			, m_initial_top(lua_gettop(&state))
#endif
		{
			assert(address >= 1);
			assert(m_initial_top >= Size::value);
		}

		basic_stack_value(basic_stack_value &&other) BOOST_NOEXCEPT
			: Size(std::move(other))
			, any_local(std::move(other))
			, m_state(other.m_state)
#ifndef NDEBUG
			, m_initial_top(other.m_initial_top)
#endif
		{
			other.m_state = nullptr;
		}

		basic_stack_value &operator = (basic_stack_value &&other) BOOST_NOEXCEPT
		{
			using boost::swap;
			swap(static_cast<Size &>(*this), static_cast<Size &>(other));
			swap(static_cast<any_local &>(*this), static_cast<any_local &>(other));
			swap(m_state, other.m_state);
#ifndef NDEBUG
			swap(m_initial_top, other.m_initial_top);
#endif
			return *this;
		}

		~basic_stack_value() BOOST_NOEXCEPT
		{
			if (!m_state)
			{
				return;
			}
#ifndef NDEBUG
			int current_top = lua_gettop(m_state);
#endif
			assert(current_top == m_initial_top);
			lua_pop(m_state, Size::value);
			assert(lua_gettop(m_state) == (m_initial_top - Size::value));
		}

		void release() BOOST_NOEXCEPT
		{
			assert(m_state);
			m_state = nullptr;
		}

		int size() const BOOST_NOEXCEPT
		{
			return Size::value;
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_state;
		}

		lua::type get_type() const BOOST_NOEXCEPT
		{
			return static_cast<lua::type>(lua_type(m_state, from_bottom()));
		}

		void assert_top() const
		{
			assert(lua_gettop(m_state) == from_bottom());
		}

		template <class Pushable>
		basic_stack_value<std::integral_constant<int, 1>> operator[](Pushable &&index)
		{
			using lua::push;
			push(*m_state, std::forward<Pushable>(index));
			lua_gettable(m_state, from_bottom());
			return basic_stack_value<std::integral_constant<int, 1>>(*m_state, lua_gettop(m_state));
		}

	private:

		lua_State *m_state;
#ifndef NDEBUG
		int m_initial_top;
#endif

		SILICIUM_DELETED_FUNCTION(basic_stack_value(basic_stack_value const &))
		SILICIUM_DELETED_FUNCTION(basic_stack_value &operator = (basic_stack_value const &))
	};

	typedef basic_stack_value<std::integral_constant<int, 1>> stack_value;
	typedef basic_stack_value<variable<int>> stack_array;

	template <class Size>
	any_local at(basic_stack_value<Size> const &array, int index)
	{
		assert(index < array.size());
		return any_local(array.from_bottom() + index);
	}

	inline void push(lua_State &L, stack_value const &value)
	{
		lua_pushvalue(&L, value.from_bottom());
	}

	inline void push(lua_State &L, stack_value &&value)
	{
		if (value.from_bottom() == lua_gettop(&L))
		{
			value.release();
		}
		else
		{
			push(L, static_cast<stack_value const &>(value));
		}
	}
}

#endif
