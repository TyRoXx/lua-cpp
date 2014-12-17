#ifndef LUACPP_REGISTER_ASYNC_FUNCTION_HPP
#define LUACPP_REGISTER_ASYNC_FUNCTION_HPP

#include "luacpp/meta_table.hpp"
#include <silicium/observable/observer.hpp>

namespace lua
{
	namespace detail
	{
		template <class Observable>
		struct async_operation : public Si::observer<typename Observable::element_type>
		{
			main_thread main;
			coroutine suspended;
			Observable observed;
			reference keep_this_alive;

			async_operation(main_thread main, coroutine suspended, Observable observed)
				: main(main)
				, suspended(std::move(suspended))
				, observed(std::move(observed))
			{
			}

			virtual void got_element(typename Observable::element_type value) SILICIUM_OVERRIDE
			{
				assert(lua_gettop(&suspended.thread()) == 0);
				auto destroy_this_at_end_of_scope = std::move(keep_this_alive);
				using lua::push;
				push(suspended.thread(), std::move(value));
				suspended.resume(1);
			}

			virtual void ended() SILICIUM_OVERRIDE
			{
				throw std::logic_error("to do");
			}
		};

		template <class ObservableFactory, class Result, class Class, class ...Args>
		stack_value register_async_function_impl(main_thread main, stack &s, ObservableFactory &&init, Result(Class::*)(Args...) const)
		{
			return register_any_function(s, [main, init](Args ...args, current_thread thread)
			{
				auto coro = pin_coroutine(main, thread);
				assert(coro && "this function cannot be called from the Lua main thread");
				auto observable = init(std::forward<Args>(args)...);
				typedef decltype(observable) observable_type;
				typedef typename observable_type::element_type element_type;
				typedef async_operation<observable_type> operation_type;

				stack s(*thread.L);
#ifndef NDEBUG
				int initial_stack_size = size(*thread.L);
#endif
				auto meta = create_default_meta_table<operation_type>(s);
				assert(initial_stack_size + 1 == size(*thread.L));

				auto object = emplace_object<operation_type>(s, meta, main, std::move(*coro), std::move(observable));
				assert(initial_stack_size + 2 == size(*thread.L));

				replace(object, meta);
				assert(initial_stack_size + 1 == size(*thread.L));

				auto &operation = assume_type<operation_type>(object);
				assert(initial_stack_size + 1 == size(*thread.L));

				operation.keep_this_alive = create_reference(main, object);
				assert(initial_stack_size + 1 == size(*thread.L));

				operation.observed.async_get_one(Si::observe_by_ref(static_cast<Si::observer<element_type> &>(operation)));
				assert(initial_stack_size + 1 == size(*thread.L));

				object.pop();
				assert(initial_stack_size == size(*thread.L));

				operation.suspended.suspend();
			});
		}
	}

	template <class ObservableFactory>
	stack_value register_async_function(main_thread main, stack &s, ObservableFactory &&init)
	{
		typedef typename std::decay<ObservableFactory>::type clean;
		return detail::register_async_function_impl(main, s, std::forward<ObservableFactory>(init), &clean::operator());
	}
}

#endif
