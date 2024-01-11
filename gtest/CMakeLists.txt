add_executable(thorin-gtest)
target_sources(thorin-gtest
    PRIVATE
        helpers.cpp
        lexer.cpp
        test.cpp
        restricted_dep_types.cpp
        helpers.h
)
target_link_libraries(thorin-gtest
    PRIVATE
        gtest_main
        libthorin
)
add_dependencies(thorin-gtest thorin_all_plugins)
gtest_discover_tests(thorin-gtest
    TEST_PREFIX "thorin."
    DISCOVERY_TIMEOUT 60
)

add_executable(thorin-regex-gtest)
target_sources(thorin-regex-gtest
    PRIVATE
        automaton.cpp
)
target_link_libraries(thorin-regex-gtest
    PRIVATE
        gtest_main
        libthorin
        automaton
)
add_dependencies(thorin-regex-gtest thorin_all_plugins)
gtest_discover_tests(thorin-regex-gtest
    TEST_PREFIX "thorin-regex-gtest."
    DISCOVERY_TIMEOUT 60
)
