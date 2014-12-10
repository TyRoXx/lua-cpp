#include <boost/test/unit_test.hpp>
#include "test_with_environment.hpp"
#include "luacpp/register_any_function.hpp"
#include "luacpp/coroutine.hpp"
#include "luacpp/meta_table.hpp"
#include <lauxlib.h>
#include <boost/optional/optional_io.hpp>
#include <silicium/source/memory_source.hpp>
#include <silicium/source/single_source.hpp>

BOOST_AUTO_TEST_CASE(lua_wrapper_create)
{
	auto s = lua::create_lua();
	BOOST_CHECK(s);
}

BOOST_AUTO_TEST_CASE(lua_wrapper_load_buffer)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	std::string const code = "return 3";
	{
		lua::stack_value const compiled = s.load_buffer(Si::make_memory_range(code), "test");
		lua::stack_value const results = s.call(compiled, lua::no_arguments(), lua::one());
		auto result = s.get_number(lua::at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_call_multret)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	std::string const code = "return 1, 2, 3";
	{
		lua::stack_value compiled = s.load_buffer(
					Si::make_memory_range(code),
					"test");
		lua::stack_array results = s.call(std::move(compiled), lua::no_arguments(), boost::none);
		BOOST_REQUIRE_EQUAL(3, results.size());
		std::vector<lua_Number> result_numbers;
		for (int i = 0; i < results.size(); ++i)
		{
			result_numbers.emplace_back(s.to_number(at(results, i)));
		}
		std::vector<lua_Number> const expected{1, 2, 3};
		BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), result_numbers.begin(), result_numbers.end());
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_call)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	{
		std::string const code = "return function (a, b, str, bool) return a * 3 + b, 7, str, not bool end";
		boost::optional<lua_Number> result_a, result_b;
		boost::optional<Si::noexcept_string> result_str;
		boost::optional<bool> result_bool;
		lua::stack_value compiled = s.load_buffer(Si::make_memory_range(code), "test");
		lua::stack_array func = s.call(compiled, lua::no_arguments(), 1);
		std::array<Si::fast_variant<lua_Number, Si::noexcept_string, bool>, 4> const arguments
		{{
			1.0,
			2.0,
			Si::noexcept_string("ff"),
			false
		}};
		lua::stack_array results = s.call(at(func, 0), Si::make_container_source(arguments), 4);
		result_a = s.get_number(at(results, 0));
		result_b = s.get_number(at(results, 1));
		result_str = s.get_string(at(results, 2));
		result_bool = s.get_boolean(at(results, 3));
		BOOST_CHECK_EQUAL(boost::make_optional(5.0), result_a);
		BOOST_CHECK_EQUAL(boost::make_optional(7.0), result_b);
		BOOST_CHECK_EQUAL(boost::optional<Si::noexcept_string>("ff"), result_str);
		BOOST_CHECK_EQUAL(boost::make_optional(true), result_bool);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

namespace
{
	int return_3(lua_State *L) BOOST_NOEXCEPT
	{
		lua_pushinteger(L, 3);
		return 1;
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_c_function)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	{
		lua::stack_value func = s.register_function(return_3);
		lua::stack_array results = s.call(func, lua::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

namespace
{
	int return_upvalues_subtracted(lua_State *L) BOOST_NOEXCEPT
	{
		lua_pushnumber(L, lua_tonumber(L, lua_upvalueindex(1)) - lua_tonumber(L, lua_upvalueindex(2)));
		return 1;
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_c_closure)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	{
		std::array<lua_Number, 2> const upvalues{{1.0, 2.0}};
		lua::stack_value func = s.register_function(return_upvalues_subtracted, Si::make_container_source(upvalues));
		lua::stack_array results = s.call(func, lua::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(-1.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_cpp_closure)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::stack_value closure = lua::register_closure(
			s,
			[bound](lua_State *L)
			{
				lua_pushnumber(L, *bound);
				return 1;
			}
		);
		BOOST_REQUIRE_EQUAL(lua::type::function, closure.get_type());
		lua::stack_array results = s.call(closure, lua::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(2.0), result);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_cpp_closure_with_upvalues)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		std::array<lua_Number, 1> const upvalues{{ 3.0 }};
		lua::stack_value closure = lua::register_closure(
			s,
			[bound](lua_State *L)
			{
				lua_pushvalue(L, lua_upvalueindex(2));
				return 1;
			},
			Si::make_container_source(upvalues)
		);
		lua::stack_array results = s.call(closure, lua::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(upvalues[0]), result);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_closure_with_converted_arguments_all_types)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::stack_value registered = lua::register_any_function(
			s,
			[&s, bound](
				bool b,
				bool const &bc,
				lua_Number n,
				lua_Number const &nc,
				lua_Integer i,
				lua_Integer const &ic,
				Si::noexcept_string str,
				Si::noexcept_string const &strc,
				char const *c_str,
				lua::any_local local,
				lua::any_local const &localc,
				void *u,
				void * const &uc
			) -> Si::noexcept_string
		{
			//The three arguments should still be on the stack,
			//for example for keeping c_str safe from the GC.
			int stack_size = lua_gettop(s.state());
			BOOST_REQUIRE_EQUAL(13, stack_size);

			BOOST_CHECK_EQUAL(true, b);
			BOOST_CHECK_EQUAL(false, bc);
			BOOST_CHECK_EQUAL(3, n);
			BOOST_CHECK_EQUAL(6, nc);
			BOOST_CHECK_EQUAL(32, i);
			BOOST_CHECK_EQUAL(64, ic);
			BOOST_CHECK_EQUAL("abc", str);
			BOOST_CHECK_EQUAL("ABC", strc);
			BOOST_REQUIRE(c_str);
			BOOST_CHECK_EQUAL(Si::noexcept_string("def"), c_str);
			BOOST_CHECK_EQUAL(-1.0, lua::from_lua_cast<lua_Number>(local));
			BOOST_CHECK_EQUAL(-2.0, lua::from_lua_cast<lua_Number>(localc));
			BOOST_CHECK_EQUAL(&s, u);
			BOOST_CHECK_EQUAL(&s, uc);
			return "it works";
		});
		std::vector<Si::fast_variant<bool, lua_Number, Si::noexcept_string, void *>> const arguments
		{
			true,
			false,
			3.0,
			6.0,
			32.0,
			64.0,
			Si::noexcept_string("abc"),
			Si::noexcept_string("ABC"),
			Si::noexcept_string("def"),
			-1.0,
			-2.0,
			static_cast<void *>(&s),
			static_cast<void *>(&s)
		};
		lua::stack_value result = s.call(registered, Si::make_container_source(arguments), std::integral_constant<int, 1>());
		boost::optional<Si::noexcept_string> str_result = s.get_string(result);
		BOOST_CHECK_EQUAL(boost::make_optional(Si::noexcept_string("it works")), str_result);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_closure_with_converted_arguments_no_arguments)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		bool called = false;
		lua::stack_value registered = lua::register_any_function(
			s,
			[&s, bound, &called]()
		{
			int stack_size = lua_gettop(s.state());
			BOOST_REQUIRE_EQUAL(0, stack_size);
			called = true;
		});
		lua::stack_value result = s.call(registered, lua::no_arguments(), std::integral_constant<int, 1>());
		BOOST_CHECK_EQUAL(lua::type::nil, s.get_type(result));
		BOOST_CHECK(called);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_closure_with_converted_arguments_mutable)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		bool called = false;
		lua::stack_value registered = lua::register_any_function(
			s,
			[&s, bound, &called]() mutable
		{
			int stack_size = lua_gettop(s.state());
			BOOST_REQUIRE_EQUAL(0, stack_size);
			called = true;
		});
		lua::stack_value result = s.call(registered, lua::no_arguments(), std::integral_constant<int, 1>());
		BOOST_CHECK_EQUAL(lua::type::nil, s.get_type(result));
		BOOST_CHECK(called);
	});
}

namespace test
{
	template <class T>
	void test_return_type(lua::stack &s, resource bound, T original)
	{
		bool called = false;
		lua::stack_value registered = lua::register_any_function(
			s,
			[&s, bound, &called, original]() -> T
		{
			int stack_size = lua_gettop(s.state());
			BOOST_REQUIRE_EQUAL(0, stack_size);
			called = true;
			return original;
		});
		lua::stack_value result = s.call(registered, lua::no_arguments(), std::integral_constant<int, 1>());
		T converted_back = lua::from_lua_cast<T>(*s.state(), result.from_bottom());
		BOOST_CHECK_EQUAL(original, converted_back);
		BOOST_CHECK(called);
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_closure_with_converted_arguments_return_types)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		test::test_return_type(s, bound, static_cast<lua_Number>(2));
		test::test_return_type(s, bound, true);
		test::test_return_type(s, bound, Si::noexcept_string("text"));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_set_meta_table)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		auto obj = s.create_user_data(1);
		{
			auto meta = s.create_table();
			s.set_element(meta, "method", s.register_function([](lua_State *L) -> int
			{
				lua_pushinteger(L, 234);
				return 1;
			}));
			s.set_element(meta, "__index", meta);
			s.set_meta_table(obj, meta);
		}
		lua::set_global(*s.state(), "obj", obj);
		boost::optional<lua_Integer> result = s.get_integer(s.call(s.load_buffer(Si::make_c_str_range("return obj:method()"), "test"), lua::no_arguments(), std::integral_constant<int, 1>()));
		BOOST_CHECK_EQUAL(boost::optional<lua_Integer>(234), result);
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_index_operator)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::stack_value table = s.create_table();
		s.set_element(table, static_cast<lua_Integer>(1), "abc");
		lua::stack_value element_1 = table[static_cast<lua_Integer>(1)];
		lua::stack_value element_2 = table[static_cast<lua_Integer>(2)];
		BOOST_CHECK_EQUAL(lua::type::string, s.get_type(element_1));
		BOOST_CHECK_EQUAL(lua::type::nil, s.get_type(element_2));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_yield)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value entry_point = coro_stack.register_function([](lua_State *L) -> int
		{
			return lua_yield(L, 0);
		});
		lua::stack::resume_result result = coro_stack.resume(std::move(entry_point), lua::no_arguments());
		BOOST_CHECK(nullptr != Si::try_get_ptr<lua::stack::yield>(result));
	});
}

namespace
{
	struct yielder
	{
		lua::main_thread main_thread;

		explicit yielder(lua::main_thread main_thread)
			: main_thread(main_thread)
		{
		}

		void operator()(lua::current_thread thread)
		{
			return yield(thread);
		}

		void yield(lua::current_thread thread)
		{
			assert(main_thread.get());
			Si::optional<lua::coroutine> coro = lua::pin_coroutine(main_thread, thread);
			BOOST_REQUIRE(coro);
			coro->suspend();
		}
	};
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_yielding_method)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value meta = lua::create_default_meta_table<yielder>(coro_stack);
		lua::add_method(coro_stack, meta, "__call", &yielder::operator ());
		lua::stack_value object = lua::emplace_object<yielder>(coro_stack, meta, lua::main_thread(*s.state()));
		lua::replace(object, meta);
		lua::stack::resume_result result = coro_stack.resume(std::move(object), lua::no_arguments());
		BOOST_CHECK(nullptr != Si::try_get_ptr<lua::stack::yield>(result));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_lua_calls_yielding_method)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());

		lua::stack_value entry_point = coro_stack.load_buffer(Si::make_c_str_range("return function (yielder) yielder:yield() end"), "test");
		BOOST_CHECK_EQUAL(0, lua_status(&coro.thread()));

		lua::stack_value entry_point_2 = coro_stack.call(entry_point, lua::no_arguments(), std::integral_constant<int, 1>());
		BOOST_CHECK_EQUAL(0, lua_status(&coro.thread()));

		lua::replace(entry_point_2, entry_point);
		BOOST_REQUIRE_EQUAL(lua::type::function, coro_stack.get_type(entry_point_2));

		lua::stack_value meta = lua::create_default_meta_table<yielder>(coro_stack);
		lua::add_method(coro_stack, meta, "yield", &yielder::yield);
		lua::stack_value object = lua::emplace_object<yielder>(coro_stack, meta, lua::main_thread(*s.state()));
		lua::replace(object, meta);
		BOOST_REQUIRE_EQUAL(lua::type::user_data, coro_stack.get_type(object));

		entry_point_2.release();
		object.release();
		coro.resume(1);
		BOOST_CHECK_EQUAL(LUA_YIELD, lua_status(&coro.thread()));
	});
}

BOOST_AUTO_TEST_CASE(lua_wrapper_coroutine_finish)
{
	test::test_with_environment([](lua::stack &s, test::resource bound)
	{
		lua::coroutine coro = lua::create_coroutine(lua::main_thread(*s.state()));
		lua::stack coro_stack(coro.thread());
		lua::stack_value entry_point = coro_stack.register_function([](lua_State *L) -> int
		{
			lua_pushinteger(L, 23);
			return 1;
		});
		lua::stack::resume_result result = coro_stack.resume(std::move(entry_point), lua::no_arguments());
		lua::stack_array * const return_values = Si::try_get_ptr<lua::stack_array>(result);
		BOOST_REQUIRE(return_values);
		BOOST_REQUIRE_EQUAL(1, return_values->size());
		BOOST_CHECK_EQUAL(static_cast<lua_Integer>(23), coro_stack.get_integer(*return_values));
	});
}
