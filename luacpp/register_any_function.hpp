#ifndef LUACPP_REGISTER_ANY_FUNCTION_HPP
#define LUACPP_REGISTER_ANY_FUNCTION_HPP

#include "luacpp/stack.hpp"
#include "luacpp/register_closure.hpp"
#include "luacpp/from_lua_cast.hpp"
#include "luacpp/coroutine.hpp"
#include <silicium/detail/integer_sequence.hpp>

namespace lua
{
	struct current_thread
	{
		lua_State *L;
		bool *suspend_requested;
	};

	inline boost::optional<coroutine> pin_coroutine(main_thread thread, current_thread potential_child_thread)
	{
		int rc = lua_pushthread(potential_child_thread.L);
		stack_value thread_on_stack(*potential_child_thread.L, lua_gettop(potential_child_thread.L));
		if (rc == 1)
		{
			return boost::none;
		}
		return coroutine(create_reference(thread, xmover{&thread_on_stack}), potential_child_thread.suspend_requested);
	}

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
			static BOOST_CONSTEXPR_OR_CONST bool consumes_stack = true;

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
		struct argument_converter<current_thread>
		{
			static BOOST_CONSTEXPR_OR_CONST bool consumes_stack = false;

			current_thread operator()(call_environment const &env, int) const
			{
				return current_thread{&env.L, env.suspend_requested};
			}
		};

		template <>
		struct argument_converter<lua_State &>
		{
			static BOOST_CONSTEXPR_OR_CONST bool consumes_stack = false;

			lua_State &operator()(call_environment const &env, int) const
			{
				return env.L;
			}
		};

		template <>
		struct argument_converter<any_local>
		{
			static BOOST_CONSTEXPR_OR_CONST bool consumes_stack = true;

			any_local operator()(call_environment const &env, int address) const
			{
				return any_local(env.L, address);
			}
		};

		template <class ...Arguments>
		struct argument_count_on_stack;

		template <class First, class ...Rest>
		struct argument_count_on_stack<First, Rest...> : std::integral_constant<int, argument_converter<First>::consumes_stack + argument_count_on_stack<Rest...>::value>
		{
		};

		template <>
		struct argument_count_on_stack<> : std::integral_constant<int, 0>
		{
		};

		BOOST_STATIC_ASSERT(argument_count_on_stack<>::value == 0);
		BOOST_STATIC_ASSERT(argument_count_on_stack<void *>::value == 1);
		BOOST_STATIC_ASSERT(argument_count_on_stack<void *, lua_State &>::value == 1);
		BOOST_STATIC_ASSERT(argument_count_on_stack<void *, lua_Integer>::value == 2);

		template <class NonVoid>
		struct caller
		{
			template <class ...Parameters, std::size_t ...Indices, class Function>
			result_or_yield call(Function &func, call_environment const &env, ranges::v3::integer_sequence<Indices...>) const
			{
				assert(!env.suspend_requested || !*env.suspend_requested); //TODO
				NonVoid result = func(argument_converter<Parameters>()(env, 1 + Indices)...);
				push(env.L, std::move(result));
				int arguments_on_stack = argument_count_on_stack<Parameters...>::value;
				if (arguments_on_stack)
				{
					lua_replace(&env.L, 1);
					lua_pop(&env.L, arguments_on_stack - 1);
				}
				return 1;
			}
		};

		template <>
		struct caller<void>
		{
			template <class ...Parameters, std::size_t ...Indices, class Function>
			result_or_yield call(Function &func, call_environment const &env, ranges::v3::integer_sequence<Indices...>) const
			{
				func(argument_converter<Parameters>()(env, 1 + Indices)...);
				int arguments_on_stack = argument_count_on_stack<Parameters...>::value;
				if (arguments_on_stack)
				{
					lua_pop(&env.L, arguments_on_stack);
				}
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
				return caller<R>().template call<Parameters...>(func, env, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
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
				return caller<R>().template call<Parameters...>(func, env, typename ranges::v3::make_integer_sequence<sizeof...(Parameters)>::type());
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
