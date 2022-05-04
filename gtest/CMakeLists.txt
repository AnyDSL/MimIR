add_executable(thorin-gtest
    lexer.cpp
    test.cpp
    for_ax.cpp
    restricted_dep_types.cpp
)

target_link_libraries(thorin-gtest gtest_main libthorin)
gtest_discover_tests (thorin-gtest TEST_PREFIX "thorin." DISCOVERY_TIMEOUT 60)
