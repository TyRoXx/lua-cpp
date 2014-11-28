add_executable(lode lode.cpp)
target_link_libraries(lode ${Boost_LIBRARIES} ${LUA_LIBRARIES})
