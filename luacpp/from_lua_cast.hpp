#ifndef LUACPP_FROM_LUA_CAST_HPP
#define LUACPP_FROM_LUA_CAST_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/stack.hpp"
#include "luacpp/reference.hpp"
#include <silicium/memory_range.hpp>

namespace lua
{
	template <class T>
	struct from_lua;

	template <>
	struct from_lua<lua_Number>
	{
		static type const lua_type = type::number;

		lua_Number operator()(lua_State &L, int address) const
		{
			return lua_tonumber(&L, address);
		}
	};

	template <>
	struct from_lua<lua_Integer>
	{
		static type const lua_type = type::number;

		lua_Integer operator()(lua_State &L, int address) const
		{
			return lua_tointeger(&L, address);
		}
	};

	template <>
	struct from_lua<bool>
	{
		static type const lua_type = type::boolean;

		bool operator()(lua_State &L, int address) const
		{
			return lua_toboolean(&L, address) != 0;
		}
	};

	template <>
	struct from_lua<void *>
	{
		static type const lua_type = type::user_data;

		void *operator()(lua_State &L, int address) const
		{
			return lua_touserdata(&L, address);
		}
	};

	template <>
	struct from_lua<Si::noexcept_string>
	{
		static type const lua_type = type::string;

		Si::noexcept_string operator()(lua_State &L, int address) const
		{
			char const *raw = lua_tostring(&L, address);
			if (!raw)
			{
				return Si::noexcept_string();
			}
			return raw;
		}
	};

	template <>
	struct from_lua<char const *>
	{
		static type const lua_type = type::string;

		char const *operator()(lua_State &L, int address) const
		{
			return lua_tostring(&L, address);
		}
	};

	template <class ...T>
	struct from_lua<Si::fast_variant<T...>>
	{
		typedef Si::fast_variant<T...> variant;

		variant operator()(lua_State &L, int address) const
		{
			static std::array<std::pair<variant, bool> (*)(any_local), sizeof...(T)> const converters =
			{{
				&try_convert<T>...
			}};
			std::pair<variant, bool> result = converters[0](any_local(L, address));
			for (auto const &converter : converters)
			{
				if (result.second)
				{
					break;
				}
				result = converter(any_local(L, address));
			}
			return std::move(result.first);
		}

	private:

		template <class To>
		static std::pair<variant, bool> try_convert(any_local from)
		{
			auto const type = get_type(from);
			bool is_correct_type = (from_lua<To>::lua_type == type);
			return std::make_pair(variant(from_lua<To>()(*from.thread(), from.from_bottom())), is_correct_type);
		}
	};

	template <>
	struct from_lua<Si::memory_range>
	{
		Si::memory_range operator()(lua_State &L, int address) const
		{
			std::size_t length = 0;
			char const *begin = lua_tolstring(&L, address, &length);
			return Si::make_memory_range(begin, begin + length);
		}
	};

	template <class T>
	T from_lua_cast(lua_State &L, int address)
	{
		return from_lua<T>()(L, address);
	}

	template <class T>
	T from_lua_cast(any_local address)
	{
		return from_lua<T>()(*address.thread(), address.from_bottom());
	}

	template <class T>
	T from_lua_cast(lua_State &L, reference const &ref)
	{
		ref.push(L);
		stack_value pushed(L, lua_gettop(&L));
		return from_lua<T>()(L, pushed.from_bottom());
	}
}

#endif
