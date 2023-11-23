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

add_executable(thorin-regex-gtest
    automaton.cpp
    ../plug/regex/pass/nfa.cpp
    ../plug/regex/pass/dfa.cpp
    ../plug/regex/pass/regex2nfa.cpp
    ../plug/regex/pass/nfa2dfa.cpp
    ../plug/regex/pass/dfamin.cpp
    ../plug/regex/pass/dfa2matcher.cpp
)
target_link_libraries(thorin-regex-gtest gtest_main libthorin)
gtest_discover_tests (thorin-regex-gtest TEST_PREFIX "thorin-regex-gtest." DISCOVERY_TIMEOUT 60)

add_dependencies(thorin-regex-gtest thorin_clos thorin_core thorin_compile thorin_math thorin_mem thorin_regex)
target_include_directories(thorin-regex-gtest PRIVATE ${CMAKE_BINARY_DIR}/include)
