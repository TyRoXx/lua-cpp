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
		{
		}

		basic_stack_value(lua_State &state, int address, Size size = Size()) BOOST_NOEXCEPT
			: Size(size)
			, any_local(state, address)
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
#ifndef NDEBUG
			, m_initial_top(other.m_initial_top)
#endif
		{
			static_cast<any_local &>(other) = any_local();
		}

		basic_stack_value &operator = (basic_stack_value &&other) BOOST_NOEXCEPT
		{
			using boost::swap;
			swap(static_cast<Size &>(*this), static_cast<Size &>(other));
			swap(static_cast<any_local &>(*this), static_cast<any_local &>(other));
#ifndef NDEBUG
			swap(m_initial_top, other.m_initial_top);
#endif
			return *this;
		}

		~basic_stack_value() BOOST_NOEXCEPT
		{
			if (!thread())
			{
				return;
			}
#ifndef NDEBUG
			int current_top = lua_gettop(thread());
#endif
			assert(current_top == m_initial_top);
			lua_pop(thread(), Size::value);
			assert(lua_gettop(thread()) == (m_initial_top - Size::value));
		}

		void release() BOOST_NOEXCEPT
		{
			assert(thread());
			static_cast<any_local &>(*this) = any_local();
		}

		int size() const BOOST_NOEXCEPT
		{
			return Size::value;
		}

		void assert_top() const
		{
			assert(thread());
			assert(lua_gettop(thread()) == from_bottom());
		}

		template <class Pushable>
		basic_stack_value<std::integral_constant<int, 1>> operator[](Pushable &&index)
		{
			using lua::push;
			assert(thread());
			push(*thread(), std::forward<Pushable>(index));
			lua_gettable(thread(), from_bottom());
			return basic_stack_value<std::integral_constant<int, 1>>(*thread(), lua_gettop(thread()));
		}

	private:

#ifndef NDEBUG
		int m_initial_top;
#endif

		SILICIUM_DELETED_FUNCTION(basic_stack_value(basic_stack_value const &))
		SILICIUM_DELETED_FUNCTION(basic_stack_value &operator = (basic_stack_value const &))
	};

	typedef basic_stack_value<std::integral_constant<int, 1>> stack_value;
	typedef basic_stack_value<variable<int>> stack_array;

	inline void replace(stack_value &replacement, stack_value &replaced)
	{
		assert(replacement.thread());
		assert(replacement.thread() == replaced.thread());
		replacement.assert_top();
		lua_replace(replacement.thread(), replaced.from_bottom());
		replacement.release();
		replacement = std::move(replaced);
	}

	template <class Size>
	any_local at(basic_stack_value<Size> const &array, int index)
	{
		assert(index < array.size());
		return any_local(*array.thread(), array.from_bottom() + index);
	}

	inline void push(lua_State &L, stack_value const &value)
	{
		if (&L == value.thread())
		{
			lua_pushvalue(&L, value.from_bottom());
		}
		else
		{
			using lua::push;
			push(*value.thread(), value);
			lua_xmove(value.thread(), &L, 1);
		}
	}

	inline void push(lua_State &L, stack_value &&value)
	{
		if (&L == value.thread() &&
			value.from_bottom() == lua_gettop(&L))
		{
			value.release();
		}
		else
		{
			push(L, static_cast<stack_value const &>(value));
		}
	}

	struct xmover
	{
		stack_value const *from;
	};

	inline void push(lua_State &L, xmover const &value)
	{
		assert(value.from);
		assert(&L != value.from->thread());
		using lua::push;
		push(*value.from->thread(), *value.from);
		lua_xmove(value.from->thread(), &L, 1);
	}
}

#endif
