#ifndef LUACPP_REGISTER_CLOSURE_HPP
#define LUACPP_REGISTER_CLOSURE_HPP

#include "luacpp/stack.hpp"
#include <silicium/source/empty.hpp>

namespace lua
{
	struct yield
	{
	};

	typedef Si::fast_variant<int, yield> result_or_yield;

	namespace detail
	{
		template <class Function>
		int call_upvalue_function(lua_State *L) BOOST_NOEXCEPT
		{
			int result;
			bool yielding = false;
			{
				Function * const f_stored = static_cast<Function *>(lua_touserdata(L, lua_upvalueindex(1)));
				assert(f_stored);
				result_or_yield command = (*f_stored)(L);
				Si::visit<void>(
					command,
					[&result](int rc)
					{
						result = rc;
					},
					[&yielding](yield)
					{
						yielding = true;
					}
				);
			}
			if (yielding)
			{
				assert(lua_gettop(L) == 0);
				return lua_yield(L, 0);
			}
			return result;
		}

		template <class Function>
		int delete_function(lua_State *L) BOOST_NOEXCEPT
		{
			Function * const function = static_cast<Function *>(lua_touserdata(L, -1));
			assert(function);
			function->~Function();
			return 0;
		}

		struct placement_destructor
		{
			template <class T>
			void operator()(T *object) const BOOST_NOEXCEPT
			{
#ifdef _MSC_VER
				//workaround for VC++ bug
				boost::ignore_unused_variable_warning(object);
#endif
				object->~T();
			}
		};
	}

	template <class Function, class UpvalueSource>
	stack_value register_closure(stack &s, Function &&f, UpvalueSource &&upvalues)
	{
		typedef typename std::decay<Function>::type clean_function;
		stack_value data = create_user_data(*s.state(), sizeof(f));
		{
			clean_function * const f_stored = static_cast<clean_function *>(to_user_data(data));
			assert(f_stored);
			new (f_stored) clean_function{std::forward<Function>(f)};
			std::unique_ptr<clean_function, detail::placement_destructor> f_stored_handle(f_stored);
			{
				stack_value meta_table = create_table(*s.state());
				//TODO: cache metatable
				{
					stack_value destructor = s.register_function(detail::delete_function<clean_function>);
					s.set_element(meta_table, "__gc", destructor);
				}
				s.set_meta_table(data, meta_table);
			}
			f_stored_handle.release();
		}
		int upvalue_count = 1;
		for (;;)
		{
			auto value = Si::get(upvalues);
			if (!value)
			{
				break;
			}
			push(*s.state(), *value);
			++upvalue_count;
		}
		data.release();
		return s.register_function_with_existing_upvalues(
			detail::call_upvalue_function<clean_function>,
			upvalue_count
		);
	}

	template <class Function>
	stack_value register_closure(stack &s, Function &&f)
	{
		return register_closure(s, std::forward<Function>(f), Si::empty_source<lua_Number>());
	}
}

#endif
