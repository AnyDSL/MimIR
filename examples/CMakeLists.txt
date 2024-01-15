add_executable(hello hello.cpp)
target_link_libraries(hello PRIVATE libthorin)
add_dependencies(hello thorin_all_plugins)
set_target_properties(hello PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/examples")
install(TARGETS hello EXPORT thorin-targets)

add_dependencies(thorin_all hello)
