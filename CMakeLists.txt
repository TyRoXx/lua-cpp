cmake_minimum_required(VERSION 2.8)
project(luacpp)

if(UNIX)
	add_definitions("-Wall -Wextra -std=c++1y -Wconversion -Wvla")
	add_definitions("-Wno-missing-field-initializers")

	option(LUACPP_PEDANTIC "pass -pedantic to the compiler (may generate useless warnings)" OFF)
	if(LUACPP_PEDANTIC)
		add_definitions("-Wpedantic")
	endif()
endif()

if(MSVC)
	#make Visual C++ 2013 build in parallel
	add_definitions("/MP")

	#we want more warnings
	add_definitions("/W4")

	#disable "'std::_Copy_n': Function call with parameters that may be unsafe"
	add_definitions("/wd4996")

	#workaround for Boost 1.55 Context error LNK2026
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
endif()

if(MSVC)
	set(Boost_USE_STATIC_LIBS ON) 
endif()
if(NOT MSVC)
	set(SILICIUM_BOOST_REQUIRED REQUIRED)
endif()
find_package(Boost ${SILICIUM_BOOST_REQUIRED} COMPONENTS unit_test_framework thread filesystem iostreams system coroutine program_options)
if(UNIX)
	add_definitions("-DBOOST_TEST_DYN_LINK")
endif()
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

if(WIN32)
	#Boost.Asio wants this for no reason
	add_definitions("-D_WIN32_WINDOWS")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Silicium REQUIRED)
include_directories(SYSTEM ${SILICIUM_INCLUDE_DIR})

find_package(Lua51 REQUIRED)
include_directories(SYSTEM ${LUA_INCLUDE_DIR})

include_directories(".")
add_subdirectory("test")
add_subdirectory("examples")
