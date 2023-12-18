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
        thorin_interface_core
        thorin_interface_compile
        thorin_interface_math
        thorin_interface_mem
)
add_dependencies(thorin-gtest
    thorin_clos
    thorin_core
    thorin_compile
    thorin_math
    thorin_mem
)
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
        thorin_regex
)
add_dependencies(thorin-regex-gtest
    thorin_clos
    thorin_compile
    thorin_math
)
gtest_discover_tests(thorin-regex-gtest
    TEST_PREFIX "thorin-regex-gtest."
    DISCOVERY_TIMEOUT 60
)
