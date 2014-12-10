#ifndef LUACPP_TEST_WITH_ENVIRONMENT_HPP
#define LUACPP_TEST_WITH_ENVIRONMENT_HPP

#include "luacpp/stack.hpp"

namespace test
{
	typedef std::shared_ptr<lua_Number> resource;

	inline void test_with_environment(std::function<void (lua::stack &, resource)> const &run)
	{
		auto res = std::make_shared<lua_Number>(2);
		{
			auto state = lua::create_lua();
			lua::stack s(*state);
			run(s, res);
			int top = lua_gettop(s.state());
			BOOST_CHECK_EQUAL(0, top);
		}
		BOOST_CHECK_EQUAL(1, res.use_count());
	}
}

#endif
