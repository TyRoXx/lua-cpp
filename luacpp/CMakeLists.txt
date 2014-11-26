file(GLOB sources "*.hpp" "*.cpp")
add_library(luacpp ${sources})
target_link_libraries(luacpp ${LUA_LIBRARIES})
