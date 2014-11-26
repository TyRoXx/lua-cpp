#ifndef BUILDSERVER_LUA_ENVIRONMENT_HPP
#define BUILDSERVER_LUA_ENVIRONMENT_HPP

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}
#include <boost/config.hpp>
#include <boost/system/system_error.hpp>
#include <memory>
#include <silicium/memory_range.hpp>
#include <silicium/config.hpp>
#include <silicium/override.hpp>
#include <silicium/noexcept_string.hpp>
#include <silicium/fast_variant.hpp>
#include <silicium/source/empty.hpp>
#include <silicium/detail/integer_sequence.hpp>

namespace lua
{
	struct lua_error_category : boost::system::error_category
	{
		virtual const char *name() const BOOST_SYSTEM_NOEXCEPT SILICIUM_OVERRIDE;
		virtual std::string message(int ev) const SILICIUM_OVERRIDE;
	};

	boost::system::error_category const &get_lua_error_category();

	struct lua_deleter
	{
		void operator()(lua_State *L) const BOOST_NOEXCEPT;
	};

	typedef std::unique_ptr<lua_State, lua_deleter> state_ptr;

	state_ptr create_lua();

	boost::system::error_code load_buffer(lua_State &L, Si::memory_range code, char const *name);

	enum class type
	{
		nil = LUA_TNIL,
		boolean = LUA_TBOOLEAN,
		light_user_data = LUA_TLIGHTUSERDATA,
		number = LUA_TNUMBER,
		string = LUA_TSTRING,
		table = LUA_TTABLE,
		function = LUA_TFUNCTION,
		user_data = LUA_TUSERDATA,
		thread = LUA_TTHREAD
	};

	inline std::ostream &operator << (std::ostream &out, type value)
	{
		switch (value)
		{
		case type::nil: return out << "nil";
		case type::boolean: return out << "boolean";
		case type::light_user_data: return out << "light_user_data";
		case type::number: return out << "number";
		case type::string: return out << "string";
		case type::table: return out << "table";
		case type::function: return out << "function";
		case type::user_data: return out << "user_data";
		case type::thread: return out << "thread";
		}
		return out << "??";
	}

	struct pushable
	{
		virtual ~pushable() BOOST_NOEXCEPT
		{
		}
		virtual void push(lua_State &L) const = 0;
	};

	inline void push(lua_State &L, pushable const &p)
	{
		p.push(L);
	}

	inline void push(lua_State &L, pushable const *p)
	{
		assert(p);
		push(L, *p);
	}

	inline void push(lua_State &L, lua_Number value) BOOST_NOEXCEPT
	{
		lua_pushnumber(&L, value);
	}

	inline void push(lua_State &L, Si::noexcept_string const &value) BOOST_NOEXCEPT
	{
		lua_pushlstring(&L, value.data(), value.size());
	}

	inline void push(lua_State &L, char const *c_str) BOOST_NOEXCEPT
	{
		lua_pushstring(&L, c_str);
	}

	inline void push(lua_State &L, bool value) BOOST_NOEXCEPT
	{
		lua_pushboolean(&L, value);
	}

	namespace detail
	{
		struct pusher
		{
			typedef void result_type;

			lua_State *L;

			template <class T>
			void operator()(T const &value) const
			{
				push(*L, value);
			}
		};
	}

	template <class ...T>
	inline void push(lua_State &L, Si::fast_variant<T...> const &value)
	{
		return Si::apply_visitor(detail::pusher{&L}, value);
	}

	struct any_local : pushable
	{
		any_local() BOOST_NOEXCEPT
			: m_from_bottom(-1)
		{
		}

		explicit any_local(int from_bottom) BOOST_NOEXCEPT
			: m_from_bottom(from_bottom)
		{
			assert(m_from_bottom >= 1);
		}

		int from_bottom() const BOOST_NOEXCEPT
		{
			return m_from_bottom;
		}

		virtual void push(lua_State &L) const SILICIUM_OVERRIDE
		{
			assert(m_from_bottom >= 1);
			lua_pushvalue(&L, m_from_bottom);
		}

	private:

		int m_from_bottom;
	};

	struct array
	{
		array(int begin, int length) BOOST_NOEXCEPT
			: m_begin(begin)
			, m_length(length)
		{
		}

		int begin() const BOOST_NOEXCEPT
		{
			return m_begin;
		}

		int length() const BOOST_NOEXCEPT
		{
			return m_length;
		}

		any_local operator[](int index) const BOOST_NOEXCEPT
		{
			assert(index < m_length);
			return any_local(m_begin + index);
		}

	private:

		int m_begin;
		int m_length;
	};

	template <type Type>
	struct typed_local : any_local
	{
		typed_local() BOOST_NOEXCEPT
		{
		}

		explicit typed_local(int from_bottom) BOOST_NOEXCEPT
			: any_local(from_bottom)
		{
		}
	};

	struct reference : pushable
	{
		reference() BOOST_NOEXCEPT
			: m_state(nullptr)
		{
		}

		explicit reference(lua_State &state, int key) BOOST_NOEXCEPT
			: m_state(&state)
			, m_key(key)
		{
		}

		reference(reference &&other) BOOST_NOEXCEPT
			: m_state(other.m_state)
			, m_key(other.m_key)
		{
			other.m_state = nullptr;
		}

		reference &operator = (reference &&other) BOOST_NOEXCEPT
		{
			std::swap(m_state, other.m_state);
			std::swap(m_key, other.m_key);
			return *this;
		}

		~reference() BOOST_NOEXCEPT
		{
			if (!m_state)
			{
				return;
			}
			luaL_unref(m_state, LUA_REGISTRYINDEX, m_key);
		}

		bool empty() const BOOST_NOEXCEPT
		{
			return !m_state;
		}

		void push() const BOOST_NOEXCEPT
		{
			assert(!empty());
			lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_key);
		}

		virtual void push(lua_State &L) const SILICIUM_OVERRIDE
		{
			assert(&L == m_state);
			push();
		}

	private:

		lua_State *m_state;
		int m_key;

		SILICIUM_DELETED_FUNCTION(reference(reference const &))
		SILICIUM_DELETED_FUNCTION(reference &operator = (reference const &))
	};

	inline void push(lua_State &, reference const &ref) BOOST_NOEXCEPT
	{
		ref.push();
	}

	template <class Pushable>
	reference create_reference(lua_State &L, Pushable const &value)
	{
		push(L, value);
		int key = luaL_ref(&L, LUA_REGISTRYINDEX);
		return reference(L, key);
	}

	struct lua_exception : std::runtime_error
	{
		explicit lua_exception(std::string message)
			: std::runtime_error(std::move(message))
		{
		}
	};

	template <class T>
	struct variable
	{
		T value;
	};

	template <class Size>
	struct basic_stack_value : private Size
	{
		basic_stack_value() BOOST_NOEXCEPT
			: m_state(nullptr)
		{
		}

		basic_stack_value(lua_State &state, int address, Size size = Size()) BOOST_NOEXCEPT
			: Size(size)
			, m_state(&state)
			, m_address(address)
#ifndef NDEBUG
			, m_initial_top(lua_gettop(&state))
#endif
		{
			assert(m_address >= 1);
			assert(m_initial_top >= Size::value);
		}

		basic_stack_value(basic_stack_value &&other) BOOST_NOEXCEPT
			: m_state(other.m_state)
			, m_address(other.m_address)
#ifndef NDEBUG
			, m_initial_top(other.m_initial_top)
#endif
		{
			other.m_state = nullptr;
		}

		~basic_stack_value() BOOST_NOEXCEPT
		{
			if (!m_state)
			{
				return;
			}
#ifndef NDEBUG
			int current_top = lua_gettop(m_state);
#endif
			assert(current_top == m_initial_top);
			lua_pop(m_state, Size::value);
			assert(lua_gettop(m_state) == (m_initial_top - Size::value));
		}

		void release() BOOST_NOEXCEPT
		{
			assert(m_state);
			m_state = nullptr;
		}

		int size() const BOOST_NOEXCEPT
		{
			return Size::value;
		}

		lua_State *state() const BOOST_NOEXCEPT
		{
			return m_state;
		}

		void push() const BOOST_NOEXCEPT
		{
			assert(m_state);
			lua_pushvalue(m_state, m_address);
		}

		int from_bottom() const BOOST_NOEXCEPT
		{
			return m_address;
		}

		lua::type get_type() const BOOST_NOEXCEPT
		{
			return static_cast<lua::type>(lua_type(m_state, m_address));
		}

	private:

		lua_State *m_state;
		int m_address;
#ifndef NDEBUG
		int m_initial_top;
#endif

		SILICIUM_DELETED_FUNCTION(basic_stack_value(basic_stack_value const &))
		SILICIUM_DELETED_FUNCTION(basic_stack_value &operator = (basic_stack_value const &))
		SILICIUM_DELETED_FUNCTION(basic_stack_value &operator = (basic_stack_value &&))
	};

	typedef basic_stack_value<std::integral_constant<int, 1>> stack_value;
	typedef basic_stack_value<variable<int>> stack_array;

	template <class Size>
	any_local at(basic_stack_value<Size> const &array, int index)
	{
		assert(index < array.size());
		return any_local(array.from_bottom() + index);
	}

	void push(lua_State &L, stack_value const &value);

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
				push(*m_state, *argument);
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

		template <class Key, class Element>
		void set_element(any_local const &table, Key &&key, Element &&element)
		{
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

	template <class T>
	struct from_lua;

	template <>
	struct from_lua<lua_Number>
	{
		lua_Number operator()(lua_State &L, int address) const
		{
			return lua_tonumber(&L, address);
		}
	};

	template <>
	struct from_lua<bool>
	{
		bool operator()(lua_State &L, int address) const
		{
			return lua_toboolean(&L, address);
		}
	};

	template <>
	struct from_lua<Si::noexcept_string>
	{
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
		char const *operator()(lua_State &L, int address) const
		{
			return lua_tostring(&L, address);
		}
	};

	template <>
	struct from_lua<reference>
	{
		reference operator()(lua_State &L, int address) const
		{
			return create_reference(L, any_local(address));
		}
	};

	template <class T>
	T from_lua_cast(lua_State &L, int address)
	{
		return from_lua<T>()(L, address);
	}

	template <class T>
	T from_lua_cast(lua_State &L, reference const &ref)
	{
		ref.push();
		stack_value pushed(L, lua_gettop(&L));
		return from_lua<T>()(L, pushed.from_bottom());
	}

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
