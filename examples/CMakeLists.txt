add_executable(hello hello.cpp)

target_link_libraries(hello libthorin)

set_target_properties(hello PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/examples")
install(TARGETS hello EXPORT install_exports)
