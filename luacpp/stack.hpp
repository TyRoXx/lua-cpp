#ifndef LUACPP_STACK_HPP
#define LUACPP_STACK_HPP

#include "luacpp/stack_value.hpp"
#include "luacpp/state.hpp"
#include "luacpp/exception.hpp"
#include <silicium/source/empty.hpp>

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

		explicit stack(state_ptr state) BOOST_NOEXCEPT
			: m_state(std::move(state))
		{
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_state.get();
		}

		stack_value load_buffer(Si::memory_range code, char const *name)
		{
			auto error = lua::load_buffer(*m_state, code, name);
			if (error)
			{
				boost::throw_exception(boost::system::system_error(error));
			}
			int top = lua_gettop(m_state.get());
			return stack_value(*m_state, top);
		}

		std::pair<error, stack_value> load_file(boost::filesystem::path const &file)
		{
			int rc = luaL_loadfile(m_state.get(), file.c_str());
			error ec = static_cast<error>(rc);
			int top = checked_top();
			return std::make_pair(ec, stack_value(*m_state, top));
		}

		template <class Pushable, class ArgumentSource>
		stack_array call(Pushable const &function, ArgumentSource &&arguments, boost::optional<int> expected_result_count)
		{
			int const top_before = checked_top();
			push(*m_state, function);
			assert(checked_top() == (top_before + 1));
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
			assert(checked_top() == (top_before + 1 + argument_count));
			int const nresults = expected_result_count ? *expected_result_count : LUA_MULTRET;
			//TODO: stack trace in case of an error
			if (lua_pcall(m_state.get(), argument_count, nresults, 0) != 0)
			{
				std::string message = lua_tostring(m_state.get(), -1);
				lua_pop(m_state.get(), 1);
				boost::throw_exception(lua_exception(std::move(message)));
			}
			int const top_after_call = checked_top();
			assert(top_after_call >= top_before);
			return stack_array(*m_state, top_before + 1, variable<int>{top_after_call - top_before});
		}

		template <class Pushable, class ArgumentSource>
		stack_value call(Pushable const &function, ArgumentSource &&arguments, std::integral_constant<int, 1> expected_result_count)
		{
			stack_array results = call(function, arguments, expected_result_count.value);
			int where = results.from_bottom();
			results.release();
			return stack_value(*m_state, where);
		}

		stack_value register_function(int (*function)(lua_State *L))
		{
			lua_pushcfunction(m_state.get(), function);
			return stack_value(*m_state, checked_top());
		}

		stack_value register_function_with_existing_upvalues(int (*function)(lua_State *L), int upvalue_count)
		{
			assert(checked_top() >= upvalue_count);
			lua_pushcclosure(m_state.get(), function, upvalue_count);
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
			return static_cast<type>(lua_type(m_state.get(), local.from_bottom()));
		}

		lua_Number to_number(any_local const &local)
		{
			return lua_tonumber(m_state.get(), local.from_bottom());
		}

		lua_Integer to_integer(any_local const &local)
		{
			return lua_tointeger(m_state.get(), local.from_bottom());
		}

		Si::noexcept_string to_string(any_local const &local)
		{
			return lua_tostring(m_state.get(), local.from_bottom());
		}

		bool to_boolean(any_local const &local)
		{
			return lua_toboolean(m_state.get(), local.from_bottom());
		}

		void *to_user_data(any_local const &local)
		{
			return lua_touserdata(m_state.get(), local.from_bottom());
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
			void *user_data = lua_newuserdata(m_state.get(), size);
			assert(size == 0 || user_data);
			return stack_value(*m_state, checked_top());
		}

		stack_value create_table(int array_size = 0, int non_array_size = 0)
		{
			lua_createtable(m_state.get(), array_size, non_array_size);
			return stack_value(*m_state, checked_top());
		}

		stack_value push_nil()
		{
			lua_pushnil(m_state.get());
			return stack_value(*m_state, checked_top());
		}

		template <class Key, class Element>
		void set_element(any_local const &table, Key &&key, Element &&element)
		{
			top_checker checker(*m_state);
			push(*m_state, std::forward<Key>(key));
			push(*m_state, std::forward<Element>(element));
			lua_settable(m_state.get(), table.from_bottom());
		}

		template <class Metatable>
		void set_meta_table(any_local const &object, Metatable &&meta)
		{
			push(*m_state, std::forward<Metatable>(meta));
			lua_setmetatable(m_state.get(), object.from_bottom());
		}

	private:

		state_ptr m_state;

		int checked_top() const BOOST_NOEXCEPT
		{
			int top = lua_gettop(m_state.get());
			assert(top >= 0);
			return top;
		}
	};

	template <class T, class Pushable, class ...Args>
	stack_value emplace_object(stack &s, Pushable &&meta_table, Args &&...args)
	{
		stack_value obj = s.create_user_data(sizeof(T));
		T * const raw_obj = static_cast<T *>(s.to_user_data(obj));
		assert(raw_obj);
		new (raw_obj) T{std::forward<Args>(args)...};
		try
		{
			s.set_meta_table(obj, std::forward<Pushable>(meta_table));
		}
		catch (...)
		{
			raw_obj->~T();
			throw;
		}
		return obj;
	}

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
