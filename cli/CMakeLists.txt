add_executable(thorin
    main.cpp)

target_link_libraries(thorin libthorin lyra CLI11::CLI11)

set_target_properties(thorin PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS thorin EXPORT install_exports)
