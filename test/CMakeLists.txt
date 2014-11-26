file(GLOB sources "*.hpp" "*.cpp" "../luacpp/*.hpp")
add_executable(unit_test ${sources})
target_link_libraries(unit_test ${Boost_LIBRARIES} ${LUA_LIBRARIES})
