#ifndef LUACPP_STACK_HPP
#define LUACPP_STACK_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/state.hpp"
#include "luacpp/exception.hpp"
#include <silicium/source/empty.hpp>
#include <silicium/fast_variant.hpp>

namespace lua
{
	struct top_checker
	{
		explicit top_checker(lua_State &lua)
			: m_lua(lua)
			, m_top_on_entry(lua_gettop(&lua))
		{
		}

		~top_checker()
		{
			int const top_on_exit = lua_gettop(&m_lua);
			assert(top_on_exit == m_top_on_entry);
		}

	private:

		lua_State &m_lua;
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

		stack_value load_buffer(Si::memory_range code, char const *name)
		{
			auto error = lua::load_buffer(*m_state, code, name);
			if (error)
			{
				boost::throw_exception(boost::system::system_error(error));
			}
			int top = lua_gettop(m_state);
			return stack_value(*m_state, top);
		}

		std::pair<error, stack_value> load_file(boost::filesystem::path const &file)
		{
			int rc = luaL_loadfile(m_state, file.c_str());
			error ec = static_cast<error>(rc);
			int top = checked_top();
			return std::make_pair(ec, stack_value(*m_state, top));
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
			int const top_before = checked_top();
			int const argument_count = push_arguments(function, arguments);
			assert(checked_top() == top_before + argument_count + 1);
			int const nresults = expected_result_count ? *expected_result_count : LUA_MULTRET;
			int const rc = lua_pcall(m_state, argument_count, nresults, 0);
			int const top_after_call = checked_top();
			assert(top_after_call >= top_before);
			if (rc == 0)
			{
				assert(top_after_call == top_before + nresults);
			}
			else
			{
				assert(top_after_call == top_before + 1);
			}
			handle_pcall_result(rc);
			return stack_array(*m_state, top_before + 1, variable<int>{top_after_call - top_before});
		}

		template <class Pushable, class ArgumentSource>
		stack_value call(Pushable const &function, ArgumentSource &&arguments, std::integral_constant<int, 1> expected_result_count)
		{
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
			handle_pcall_result(rc);
			int const result_count = checked_top();
			return stack_array(*m_state, 1, variable<int>{result_count});
		}

		stack_value register_function(int (*function)(lua_State *L))
		{
			lua_pushcfunction(m_state, function);
			return stack_value(*m_state, checked_top());
		}

		stack_value register_function_with_existing_upvalues(int (*function)(lua_State *L), int upvalue_count)
		{
			assert(checked_top() >= upvalue_count);
			lua_pushcclosure(m_state, function, upvalue_count);
			return stack_value(*m_state, checked_top());
		}

		template <class UpvalueSource>
		stack_value register_function(int (*function)(lua_State *L), UpvalueSource &&values)
		{
#ifndef NDEBUG
			int const initial_top = checked_top();
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
				assert(checked_top() == (initial_top + upvalue_count));
			}
			return register_function_with_existing_upvalues(function, upvalue_count);
		}

		type get_type(any_local const &local)
		{
			return static_cast<type>(lua_type(m_state, local.from_bottom()));
		}

		lua_Number to_number(any_local const &local)
		{
			return lua_tonumber(m_state, local.from_bottom());
		}

		lua_Integer to_integer(any_local const &local)
		{
			return lua_tointeger(m_state, local.from_bottom());
		}

		Si::noexcept_string to_string(any_local const &local)
		{
			return lua_tostring(m_state, local.from_bottom());
		}

		bool to_boolean(any_local const &local)
		{
			return lua_toboolean(m_state, local.from_bottom());
		}

		void *to_user_data(any_local const &local)
		{
			return lua_touserdata(m_state, local.from_bottom());
		}

		boost::optional<lua_Number> get_number(any_local const &local)
		{
			type const t = get_type(local);
			if (t != type::number)
			{
				return boost::none;
			}
			return to_number(local);
		}

		boost::optional<lua_Integer> get_integer(any_local const &local)
		{
			type const t = get_type(local);
			if (t != type::number)
			{
				return boost::none;
			}
			return to_integer(local);
		}

		boost::optional<Si::noexcept_string> get_string(any_local const &local)
		{
			type const t = get_type(local);
			if (t != type::string)
			{
				return boost::none;
			}
			return to_string(local);
		}

		boost::optional<bool> get_boolean(any_local const &local)
		{
			type const t = get_type(local);
			if (t != type::boolean)
			{
				return boost::none;
			}
			return to_boolean(local);
		}

		stack_value create_user_data(std::size_t size)
		{
			void *user_data = lua_newuserdata(m_state, size);
			assert(size == 0 || user_data);
			return stack_value(*m_state, checked_top());
		}

		stack_value create_table(int array_size = 0, int non_array_size = 0)
		{
			lua_createtable(m_state, array_size, non_array_size);
			return stack_value(*m_state, checked_top());
		}

		stack_value push_nil()
		{
			lua_pushnil(m_state);
			return stack_value(*m_state, checked_top());
		}

		template <class Key, class Element>
		void set_element(any_local const &table, Key &&key, Element &&element)
		{
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

		int checked_top() const BOOST_NOEXCEPT
		{
			int top = lua_gettop(m_state);
			assert(top >= 0);
			return top;
		}

		template <class Pushable, class ArgumentSource>
		int push_arguments(Pushable &&function, ArgumentSource &&arguments)
		{
			push(*m_state, std::forward<Pushable>(function));
			int const top_before = checked_top();
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
			assert(checked_top() == (top_before + argument_count));
			return argument_count;
		}

		void handle_pcall_result(int rc)
		{
			if (rc == 0)
			{
				return;
			}
			//TODO: stack trace in case of an error
			std::string message = lua_tostring(m_state, -1);
			lua_pop(m_state, 1);
			boost::throw_exception(lua_exception(std::move(message)));
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
}

#endif
