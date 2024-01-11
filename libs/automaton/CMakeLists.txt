add_library(automaton)
target_sources(automaton
    PRIVATE
        src/automaton/dfa.cpp
        src/automaton/dfamin.cpp
        src/automaton/nfa2dfa.cpp
        src/automaton/nfa.cpp
    PUBLIC
        FILE_SET automaton_headers
        BASE_DIRS include
        TYPE HEADERS
        FILES
            include/automaton/automaton.h
            include/automaton/dfa.h
            include/automaton/dfamin.h
            include/automaton/nfa2dfa.h
            include/automaton/nfa.h
)
target_compile_features(automaton PUBLIC cxx_std_20)
target_link_libraries(automaton
    PUBLIC
        absl::flat_hash_map absl::flat_hash_set
)
install(
    TARGETS automaton
    EXPORT thorin-targets
    FILE_SET automaton_headers
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
