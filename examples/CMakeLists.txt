add_executable(hello hello.cpp)
target_link_libraries(hello PRIVATE thorin_interface_mem)
set_target_properties(hello PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/examples")
install(TARGETS hello EXPORT thorin-targets)
