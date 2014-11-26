#ifndef LUACPP_REGISTER_CLOSURE_HPP
#define LUACPP_REGISTER_CLOSURE_HPP

#include "luacpp/stack.hpp"
#include <silicium/source/empty.hpp>

namespace lua
{
	namespace detail
	{
		template <class Function>
		int call_upvalue_function(lua_State *L) BOOST_NOEXCEPT
		{
			Function * const f_stored = static_cast<Function *>(lua_touserdata(L, lua_upvalueindex(1)));
			assert(f_stored);
			return (*f_stored)(L);
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
				object->~T();
			}
		};
	}

	template <class Function, class UpvalueSource>
	auto register_closure(stack &s, Function &&f, UpvalueSource &&upvalues)
	{
		typedef typename std::decay<Function>::type clean_function;
		stack_value data = s.create_user_data(sizeof(f));
		{
			clean_function * const f_stored = static_cast<clean_function *>(s.to_user_data(any_local(data.from_bottom())));
			assert(f_stored);
			new (f_stored) clean_function{std::forward<Function>(f)};
			std::unique_ptr<clean_function, detail::placement_destructor> f_stored_handle(f_stored);
			{
				stack_value meta_table = s.create_table();
				//TODO: cache metatable
				{
					stack_value destructor = s.register_function(detail::delete_function<clean_function>);
					s.set_element(any_local(meta_table.from_bottom()), "__gc", destructor);
				}
				s.set_meta_table(any_local(data.from_bottom()), meta_table);
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
