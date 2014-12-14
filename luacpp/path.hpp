#ifndef LUACPP_PATH_HPP
#define LUACPP_PATH_HPP

#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/filesystem/path.hpp>
#include <silicium/config.hpp>

namespace lua
{
	SILICIUM_USE_RESULT
	inline std::string to_utf8(boost::filesystem::path const &p)
	{
		boost::filesystem::detail::utf8_codecvt_facet utf8;
		return p.string(utf8);
	}
}

#endif
