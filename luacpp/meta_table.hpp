#ifndef LUACPP_META_TABLE_HPP
#define LUACPP_META_TABLE_HPP

#include "luacpp/register_any_function.hpp"
#include "luacpp/stack.hpp"
#include "luacpp/stack_value.hpp"

namespace lua
{
	template <class T, class Pushable, class ...Args>
	stack_value emplace_object(stack &s, Pushable &&meta_table, Args &&...args)
	{
		stack_value obj = create_user_data(*s.state(), sizeof(T));
		T * const raw_obj = static_cast<T *>(to_user_data(obj));
		assert(raw_obj);
		new (raw_obj) T{std::forward<Args>(args)...};
		try
		{
			set_meta_table(obj, std::forward<Pushable>(meta_table));
		}
		catch (...)
		{
			raw_obj->~T();
			throw;
		}
		return obj;
	}

	template <class T>
	lua::stack_value create_default_meta_table(lua::stack &s)
	{
		lua::stack_value meta = create_table(*s.state());
		set_element(meta, "__index", meta);
		set_element(meta, "__metatable", "USERDATA");
		set_element(meta, "__gc", lua::register_function(*s.state(), [](lua_State *L) -> int
		{
			T *obj = static_cast<T *>(lua_touserdata(L, -1));
			assert(obj);
			obj->~T();
			return 0;
		}));
		return meta;
	}

	namespace detail
	{
#define LUA_CPP_DETAIL_REGISTER_METHOD(constness) \
		template <class Result, class Class, class ...Args> \
		stack_value register_method(lua::stack &s, Result (Class::*method)(Args...) constness) \
		{ \
			return register_any_function(s, [method](void *this_, Args ...args) -> Result \
			{ \
				Class * const raw_this = static_cast<Class *>(this_); \
				assert(raw_this); \
				return (raw_this->*method)(std::forward<Args>(args)...); \
			}); \
		}

		LUA_CPP_DETAIL_REGISTER_METHOD(BOOST_PP_EMPTY())
		LUA_CPP_DETAIL_REGISTER_METHOD(const)
#undef LUA_CPP_DETAIL_REGISTER_METHOD

		template <class T>
		struct from_user_data;

		template <class T>
		struct from_user_data<T &>
		{
			T &operator()(void *user_data) const
			{
				assert(user_data);
				return *static_cast<T *>(user_data);
			}
		};

		template <class T>
		struct from_user_data<T *>
		{
			T *operator()(void *user_data) const
			{
				assert(user_data);
				return static_cast<T *>(user_data);
			}
		};

#define LUA_CPP_DETAIL_REGISTER_METHOD_FROM_FUNCTOR(constness, mutableness) \
		template <class Functor, class Result, class Class, class Arg0, class ...Args> \
		stack_value register_method_from_functor(lua::stack &s, Functor &&functor, Result (Class::*)(Arg0, Args...) constness) \
		{ \
			return register_any_function(s, [functor](void *this_, Args ...args) mutableness -> Result \
			{ \
				assert(this_); \
				auto &&raw_this = from_user_data<Arg0>()(this_); \
				return functor(raw_this, std::forward<Args>(args)...); \
			}); \
		}

		LUA_CPP_DETAIL_REGISTER_METHOD_FROM_FUNCTOR(     , mutable)
		LUA_CPP_DETAIL_REGISTER_METHOD_FROM_FUNCTOR(const,        )
#undef LUA_CPP_DETAIL_REGISTER_METHOD_FROM_FUNCTOR

		template <class Functor>
		stack_value register_method(lua::stack &s, Functor &&functor)
		{
			typedef typename std::decay<Functor>::type clean;
			return register_method_from_functor(s, std::forward<Functor>(functor), &clean::operator());
		}
	}

	template <class MetaTable, class Name, class Function>
	void add_method(lua::stack &s, MetaTable &&meta, Name &&name, Function &&function)
	{
		set_element(
			std::forward<MetaTable>(meta),
			std::forward<Name>(name),
			detail::register_method(s, std::forward<Function>(function))
		);
	}

	template <class T>
	T &assume_type(any_local const &local)
	{
		void *data = lua_touserdata(local.thread(), local.from_bottom());
		assert(data);
		return *static_cast<T *>(data);
	}
}

#endif
