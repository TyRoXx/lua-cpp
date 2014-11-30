#ifndef LUACPP_REGISTER_ANY_FUNCTION_HPP
#define LUACPP_REGISTER_ANY_FUNCTION_HPP

#include "luacpp/stack.hpp"
#include "luacpp/register_closure.hpp"
#include "luacpp/from_lua_cast.hpp"
#include "luacpp/coroutine.hpp"
#include <silicium/detail/integer_sequence.hpp>

namespace lua
{
	namespace detail
	{
		struct call_environment
		{
			lua_State &L;
			bool *suspend_requested;
		};

		template <class T>
		struct argument_converter
		{
			T operator()(call_environment const &env, int address) const
			{
				return from_lua_cast<T>(env.L, address);
			}
		};

		template <class T>
		struct argument_converter<T const &> : argument_converter<T>
		{
		};

		template <>
		struct argument_converter<coroutine>
		{
			coroutine operator()(call_environment const &env, int) const
			{
				return coroutine(env.L, env.suspend_requested);
			}
		};

		template <class ...Parameters, std::size_t ...Indices, class Function>
		auto call_with_converted_arguments(Function &func, call_environment const &env, ranges::v3::integer_sequence<Indices...>)
		{
			return func(argument_converter<Parameters>()(env, 1 + Indices)...);
		}

		template <class NonVoid>
		struct caller
		{
			template <class F, class ...Arguments>
			result_or_yield operator()(call_environment const &env, F const &f, Arguments &&...args) const
			{
				assert(!env.suspend_requested || !*env.suspend_requested); //TODO
				NonVoid result = f(std::forward<Arguments>(args)...);
				push(env.L, std::move(result));
				if (sizeof...(Arguments))
				{
					int const top = lua_gettop(&env.L);
					lua_replace(&env.L, (top - sizeof...(Arguments)));
					lua_pop(&env.L, (sizeof...(Arguments) - 1));
				}
				return 1;
			}
		};

		template <>
		struct caller<void>
		{
			template <class F, class ...Arguments>
			result_or_yield operator()(call_environment const &env, F const &f, Arguments &&...args) const
			{
				f(std::forward<Arguments>(args)...);
				lua_pop(&env.L, sizeof...(Arguments));
				if (env.suspend_requested && *env.suspend_requested)
				{
					return yield();
				}
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
			](lua_State *L)
			{
				bool suspend_requested = false;
				call_environment env{*L, &suspend_requested};
				return caller<R>()(env, [
#ifdef _MSC_VER
					func, &env
#else
				&
#endif
				]()
				{
					return call_with_converted_arguments<Parameters...>(func, env, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
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
			](lua_State *L) mutable
			{
				bool suspend_requested = false;
				call_environment env{*L, &suspend_requested};
				return caller<R>()(env, [&]()
				{
					return call_with_converted_arguments<Parameters...>(func, env, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
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
