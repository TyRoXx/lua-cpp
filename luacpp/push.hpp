#ifndef LUACPP_PUSH_HPP
#define LUACPP_PUSH_HPP

#include "luacpp/type.hpp"
#include <cassert>
#include <silicium/noexcept_string.hpp>
#include <silicium/config.hpp>
#include <silicium/fast_variant.hpp>
#include <silicium/memory_range.hpp>

namespace lua
{
	struct pushable
	{
		virtual ~pushable() BOOST_NOEXCEPT
		{
		}
		virtual void push(lua_State &L) const = 0;
	};

	inline void push(lua_State &L, pushable const &p)
	{
		p.push(L);
	}

	inline void push(lua_State &L, pushable const *p)
	{
		assert(p);
		push(L, *p);
	}

	inline void push(lua_State &L, lua_Number value) BOOST_NOEXCEPT
	{
		lua_pushnumber(&L, value);
	}

	inline void push(lua_State &L, lua_Integer value) BOOST_NOEXCEPT
	{
		lua_pushinteger(&L, value);
	}

	inline void push(lua_State &L, char value) BOOST_NOEXCEPT
	{
		lua_pushinteger(&L, value);
	}

	inline void push(lua_State &L, Si::noexcept_string const &value) BOOST_NOEXCEPT
	{
		lua_pushlstring(&L, value.data(), value.size());
	}

	inline void push(lua_State &L, char const *c_str) BOOST_NOEXCEPT
	{
		lua_pushstring(&L, c_str);
	}

	inline void push(lua_State &L, Si::memory_range string) BOOST_NOEXCEPT
	{
		lua_pushlstring(&L, string.begin(), string.size());
	}

	inline void push(lua_State &L, bool value) BOOST_NOEXCEPT
	{
		lua_pushboolean(&L, value);
	}

	inline void push(lua_State &L, void *value) BOOST_NOEXCEPT
	{
		lua_pushlightuserdata(&L, value);
	}

	struct nil
	{
	};

	inline void push(lua_State &L, nil)
	{
		lua_pushnil(&L);
	}

	///Declared, but never defined because you should not call push like this.
	///Without this declaration there might be an implicit conversion to bool.
	void push(lua_State &L, void const *);

	template <class PushFunction, class = decltype(std::declval<PushFunction>()(std::declval<lua_State &>()))>
	void push(lua_State &L, PushFunction &&push_one)
	{
		using lua::push;
		push(L, std::forward<PushFunction>(push_one)(L));
	}

	namespace detail
	{
		struct pusher
		{
			typedef void result_type;

			lua_State *L;

			template <class T>
			void operator()(T const &value) const
			{
				push(*L, value);
			}
		};
	}

	template <class ...T>
	inline void push(lua_State &L, Si::fast_variant<T...> const &value)
	{
		return Si::apply_visitor(detail::pusher{&L}, value);
	}

	struct any_local : pushable
	{
		any_local() BOOST_NOEXCEPT
			: m_thread(nullptr)
			, m_from_bottom(-1)
		{
		}

		explicit any_local(lua_State &thread, int from_bottom) BOOST_NOEXCEPT
			: m_thread(&thread)
			, m_from_bottom(from_bottom)
		{
			assert(m_from_bottom >= 1);
		}

		lua_State *thread() const BOOST_NOEXCEPT
		{
			return m_thread;
		}

		int from_bottom() const BOOST_NOEXCEPT
		{
			return m_from_bottom;
		}

		lua::type get_type() const BOOST_NOEXCEPT
		{
			return static_cast<lua::type>(lua_type(thread(), from_bottom()));
		}

		void swap(any_local &other) BOOST_NOEXCEPT
		{
			using boost::swap;
			swap(m_thread, other.m_thread);
			swap(m_from_bottom, other.m_from_bottom);
		}

		virtual void push(lua_State &L) const SILICIUM_OVERRIDE
		{
			assert(m_from_bottom >= 1);
			if (&L == m_thread)
			{
				lua_pushvalue(&L, m_from_bottom);
			}
			else
			{
				lua_pushvalue(m_thread, m_from_bottom);
				lua_xmove(m_thread, &L, 1);
			}
		}

	private:

		lua_State *m_thread;
		int m_from_bottom;
	};

	template <type Type>
	struct typed_local : any_local
	{
		typed_local() BOOST_NOEXCEPT
		{
		}

		explicit typed_local(int from_bottom) BOOST_NOEXCEPT
			: any_local(from_bottom)
		{
		}
	};

	template <class Pushable>
	void set_global(lua_State &L, char const *name, Pushable &&value)
	{
		push(L, std::forward<Pushable>(value));
		lua_setglobal(&L, name);
	}
}

#endif
