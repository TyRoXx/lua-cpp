#ifndef LUACPP_REGISTER_ANY_FUNCTION_HPP
#define LUACPP_REGISTER_ANY_FUNCTION_HPP

#include "luacpp/stack.hpp"
#include "luacpp/register_closure.hpp"
#include "luacpp/from_lua_cast.hpp"
#include <silicium/detail/integer_sequence.hpp>

namespace lua
{
	namespace detail
	{
		template <class T>
		struct argument_converter
		{
			T operator()(lua_State &L, int address) const
			{
				return from_lua_cast<T>(L, address);
			}
		};

		template <class T>
		struct argument_converter<T const &> : argument_converter<T>
		{
		};

		template <class ...Parameters, std::size_t ...Indices, class Function>
		auto call_with_converted_arguments(Function &func, lua_State &L, ranges::v3::integer_sequence<Indices...>)
		{
			return func(argument_converter<Parameters>()(L, 1 + Indices)...);
		}

		template <class NonVoid>
		struct caller
		{
			template <class F, class ...Arguments>
			int operator()(lua_State &L, F const &f, Arguments &&...args) const
			{
				NonVoid result = f(std::forward<Arguments>(args)...);
				lua_pop(&L, sizeof...(Arguments));
				push(L, std::move(result));
				return 1;
			}
		};

		template <>
		struct caller<void>
		{
			template <class F, class ...Arguments>
			int operator()(lua_State &L, F const &f, Arguments &&...args) const
			{
				f(std::forward<Arguments>(args)...);
				lua_pop(&L, sizeof...(Arguments));
				return 0;
			}
		};

		template <class F, class R, class ...Parameters>
		stack_value register_any_function_helper(stack &s, F func, R (F::*)(Parameters...) const)
		{
			return register_closure(s, [func
#ifndef _MSC_VER
				= std::move(func)
#endif
			](lua_State *L) -> int
			{
				return caller<R>()(*L, [
#ifdef _MSC_VER
					func, L
#else
				&
#endif
				]()
				{
					return call_with_converted_arguments<Parameters...>(func, *L, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
				});
			});
		}

		template <class F, class R, class ...Parameters>
		stack_value register_any_function_helper(stack &s, F func, R (F::*)(Parameters...))
		{
			return register_closure(s, [func
#ifndef _MSC_VER
				= std::move(func)
#endif
			](lua_State *L) mutable -> int
			{
				return caller<R>()(*L, [&]()
				{
					return call_with_converted_arguments<Parameters...>(func, *L, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
				});
			});
		}
	}

	template <class Function>
	stack_value register_any_function(stack &s, Function &&f)
	{
		typedef typename std::decay<Function>::type clean_function;
		auto call_operator = &clean_function::operator();
		return detail::register_any_function_helper<clean_function>(s, std::forward<Function>(f), call_operator);
	}
}

#endif
