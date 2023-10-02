add_executable(hello hello.cpp)

target_link_libraries(hello libthorin)

set_target_properties(hello PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/examples")
add_dependencies(hello compile core)
install(TARGETS hello EXPORT thorin-targets)
