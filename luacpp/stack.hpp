#ifndef LUACPP_STACK_HPP
#define LUACPP_STACK_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/state.hpp"
#include "luacpp/pcall.hpp"
#include "luacpp/exception.hpp"
#include <silicium/source/empty.hpp>
#include <silicium/fast_variant.hpp>

namespace lua
{
	struct top_checker : boost::noncopyable
	{
		explicit top_checker(lua_State &lua)
			: m_lua(&lua)
			, m_top_on_entry(lua_gettop(&lua))
		{
		}

		~top_checker()
		{
			int const top_on_exit = lua_gettop(m_lua);
			assert(top_on_exit == m_top_on_entry);
		}

	private:

		lua_State *m_lua;
		int m_top_on_entry;
	};

	struct stack
	{
		stack() BOOST_NOEXCEPT
		{
		}

		explicit stack(lua_State &state) BOOST_NOEXCEPT
			: m_state(&state)
		{
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_state;
		}

		template <class Pushable, class ArgumentSource>
		stack_array call(Pushable const &function, ArgumentSource &&arguments, boost::optional<int> expected_result_count)
		{
#ifndef NDEBUG
			{
				int status = lua_status(m_state);
				assert(status != LUA_YIELD);
				assert(status == 0);
			}
#endif
			int const top_before = size(*m_state);
			int const argument_count = push_arguments(function, arguments);
			assert(size(*m_state) == top_before + argument_count + 1);
			int const nresults = expected_result_count ? *expected_result_count : LUA_MULTRET;
			int const rc = lua_pcall(m_state, argument_count, nresults, 0);
			int const top_after_call = size(*m_state);
			assert(top_after_call >= top_before);
			if (rc == 0)
			{
				assert(!expected_result_count || (top_after_call == top_before + nresults));
			}
			else
			{
				assert(top_after_call == top_before + 1);
			}
			handle_pcall_result(*m_state, rc);
			return stack_array(*m_state, top_before + 1, variable<int>{top_after_call - top_before});
		}

		template <class Pushable, class ArgumentSource>
		stack_value call(Pushable const &function, ArgumentSource &&arguments, std::integral_constant<int, 1> expected_result_count)
		{
#ifdef _MSC_VER
			boost::ignore_unused_variable_warning(expected_result_count);
#endif
			stack_array results = call(function, arguments, expected_result_count.value);
			assert(results.size() == 1);
			int where = results.from_bottom();
			results.release();
			return stack_value(*m_state, where);
		}

		struct yield
		{
		};

		typedef Si::fast_variant<stack_array, yield> resume_result;

		template <class ArgumentSource>
		resume_result resume(stack_value function, ArgumentSource &&arguments)
		{
#ifndef NDEBUG
			{
				int status = lua_status(m_state);
				assert(status != LUA_YIELD);
				assert(status == 0);
			}
#endif
			int const argument_count = push_arguments(std::move(function), arguments);
			int const rc = lua_resume(m_state, argument_count);
			if (rc == LUA_YIELD)
			{
				return yield();
			}
			handle_pcall_result(*m_state, rc);
			int const result_count = size(*m_state);
			return stack_array(*m_state, 1, variable<int>{result_count});
		}

		stack_value register_function(int (*function)(lua_State *L))
		{
			lua_pushcfunction(m_state, function);
			return stack_value(*m_state, size(*m_state));
		}

		stack_value register_function_with_existing_upvalues(int (*function)(lua_State *L), int upvalue_count)
		{
			assert(size(*m_state) >= upvalue_count);
			lua_pushcclosure(m_state, function, upvalue_count);
			return stack_value(*m_state, size(*m_state));
		}

		template <class UpvalueSource>
		stack_value register_function(int (*function)(lua_State *L), UpvalueSource &&values)
		{
#ifndef NDEBUG
			int const initial_top = size(*m_state);
#endif
			int upvalue_count = 0;
			for (;;)
			{
				auto value = Si::get(values);
				if (!value)
				{
					break;
				}
				push(*m_state, *value);
				++upvalue_count;
				assert(size(*m_state) == (initial_top + upvalue_count));
			}
			return register_function_with_existing_upvalues(function, upvalue_count);
		}

		template <class Key, class Element>
		void set_element(any_local const &table, Key &&key, Element &&element)
		{
			assert(get_type(table) == lua::type::table);
			top_checker checker(*m_state);
			push(*m_state, std::forward<Key>(key));
			push(*m_state, std::forward<Element>(element));
			lua_settable(m_state, table.from_bottom());
		}

		template <class Metatable>
		void set_meta_table(any_local const &object, Metatable &&meta)
		{
			push(*m_state, std::forward<Metatable>(meta));
			lua_setmetatable(m_state, object.from_bottom());
		}

	private:

		lua_State *m_state;

		template <class Pushable, class ArgumentSource>
		int push_arguments(Pushable &&function, ArgumentSource &&arguments)
		{
			push(*m_state, std::forward<Pushable>(function));
			int const top_before = size(*m_state);
			int argument_count = 0;
			for (;;)
			{
				auto argument = Si::get(arguments);
				if (!argument)
				{
					break;
				}
				push(*m_state, std::move(*argument));
				++argument_count;
			}
			assert(size(*m_state) == (top_before + argument_count));
			return argument_count;
		}
	};

	inline Si::empty_source<pushable *> no_arguments()
	{
		return {};
	}

	inline std::integral_constant<int, 1> one()
	{
		return {};
	}

	inline stack_value push_nil(lua_State &stack) BOOST_NOEXCEPT
	{
		lua_pushnil(&stack);
		return stack_value(stack, size(stack));
	}

	inline stack_value create_user_data(lua_State &stack, std::size_t data_size)
	{
		void *user_data = lua_newuserdata(&stack, data_size);
		assert(data_size == 0 || user_data);
		return stack_value(stack, size(stack));
	}

	inline stack_value create_table(lua_State &stack, int array_size = 0, int non_array_size = 0)
	{
		lua_createtable(&stack, array_size, non_array_size);
		return stack_value(stack, size(stack));
	}
}

#endif
