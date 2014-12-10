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

	template <class T>
	lua::stack_value create_default_meta_table(lua::stack &s)
	{
		lua::stack_value meta = s.create_table();
		s.set_element(meta, "__index", meta);
		s.set_element(meta, "__gc", s.register_function([](lua_State *L) -> int
		{
			T *obj = static_cast<T *>(lua_touserdata(L, -1));
			assert(obj);
			obj->~T();
			return 0;
		}));
		return meta;
	}

	template <class MetaTable, class Name, class R, class Class, class ...Args>
	void add_method(lua::stack &s, MetaTable &&meta, Name &&name, R (Class::*method)(Args...))
	{
		assert(s.get_type(meta) == type::table);
		stack_value registered_method = lua::register_any_function(s, [method](void *this_, Args ...args) -> R
		{
			Class * const raw_this = static_cast<Class *>(this_);
			assert(raw_this);
			return (raw_this->*method)(std::forward<Args>(args)...);
		});
		assert(s.get_type(meta) == type::table);
		assert(s.get_type(registered_method) == type::function);
		s.set_element(
			meta,
			std::forward<Name>(name),
			std::move(registered_method)
		);
	}

	template <class MetaTable, class Name, class R, class Class, class ...Args>
	void add_method(lua::stack &s, MetaTable &&meta, Name &&name, R (Class::*method)(Args...) const)
	{
		s.set_element(
			meta,
			std::forward<Name>(name),
			lua::register_any_function(s, [method](void *this_, Args ...args) -> R
		{
			Class * const raw_this = static_cast<Class *>(this_);
			assert(raw_this);
			return (raw_this->*method)(std::forward<Args>(args)...);
		}));
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
