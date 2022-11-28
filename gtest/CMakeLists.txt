add_executable(thorin-gtest
    helpers.cpp
    helpers.h
    lexer.cpp
    test.cpp
    restricted_dep_types.cpp
)

target_link_libraries(thorin-gtest gtest_main libthorin)
gtest_discover_tests (thorin-gtest TEST_PREFIX "thorin." DISCOVERY_TIMEOUT 60)

add_dependencies(thorin-gtest thorin_clos thorin_core thorin_compile thorin_math thorin_mem)
target_include_directories(thorin-gtest PRIVATE ${CMAKE_BINARY_DIR}/include)
