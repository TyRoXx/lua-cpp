#ifndef LUACPP_EXCEPTION_HPP
#define LUACPP_EXCEPTION_HPP

#include <stdexcept>

namespace lua
{
	struct lua_exception : std::runtime_error
	{
		explicit lua_exception(std::string message)
			: std::runtime_error(std::move(message))
		{
		}
	};
}

#endif
