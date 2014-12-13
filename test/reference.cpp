#include <boost/test/unit_test.hpp>
#include "luacpp/reference.hpp"
#include "luacpp/state.hpp"
#include "luacpp/stack.hpp"
#include "luacpp/load.hpp"

BOOST_AUTO_TEST_CASE(lua_wrapper_reference)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::stack s(L);
	std::string const code = "return 3";
	lua::reference const ref = lua::create_reference(lua::main_thread(L), lua::load_buffer(L, Si::make_memory_range(code), "test").value());
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
	BOOST_REQUIRE(!ref.empty());
	{
		lua::stack_array results = s.call(ref, lua::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}
