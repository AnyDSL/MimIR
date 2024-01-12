add_library(automaton)
target_sources(automaton
    PRIVATE
        dfa.cpp
        dfamin.cpp
        nfa2dfa.cpp
        nfa.cpp
)
target_include_directories(automaton
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_compile_features(automaton PUBLIC cxx_std_20)
target_link_libraries(automaton
    PUBLIC
        absl::flat_hash_map absl::flat_hash_set
)

install(
    TARGETS automaton
    EXPORT thorin-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/automaton
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
