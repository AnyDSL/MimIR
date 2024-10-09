add_executable(hello hello.cpp)
target_link_libraries(hello PRIVATE libmim)
add_dependencies(hello mim_all_plugins)
set_target_properties(hello PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/examples")
install(TARGETS hello EXPORT mim-targets)

add_dependencies(mim_all hello)
