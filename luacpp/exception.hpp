#ifndef LUACPP_EXCEPTION_HPP
#define LUACPP_EXCEPTION_HPP

#include <stdexcept>
#include <boost/config.hpp>

namespace lua
{
	struct lua_exception : std::runtime_error
	{
		explicit lua_exception(int rc, std::string message)
			: std::runtime_error(std::move(message))
			, m_rc(rc)
		{
		}

		int code() const BOOST_NOEXCEPT
		{
			return m_rc;
		}

	private:

		int m_rc;
	};
}

#endif
