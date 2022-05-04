add_library(thorin_foo MODULE
    foo.cpp
    foo.h
)

set_target_properties(thorin_foo
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

target_link_libraries(thorin_foo libthorin)

function(add_thorin_dialect THORIN_FILE)
    cmake_path(GET THORIN_FILE STEM DIALECT)
    set(DIALECT_MD ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.md)
    set(DIALECT_H  ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.h)

    list(APPEND DIALECTS    "${DIALECT}"   )
    list(APPEND DIALECT_MDS "${DIALECT_MD}")
    set(DIALECTS    ${DIALECTS}    PARENT_SCOPE)
    set(DIALECT_MDS ${DIALECT_MDS} PARENT_SCOPE)

    add_custom_command(
        OUTPUT ${DIALECT_MD} ${DIALECT_H}
        COMMAND thorin -e md -e h ${THORIN_FILE}
        MAIN_DEPENDENCY ${THORIN_FILE}
        DEPENDS thorin
        COMMENT "Bootstrapping Thorin dialect '${DIALECT}' from '${THORIN_FILE}'")

    add_custom_target(${DIALECT} ALL DEPENDS ${DIALECT_MD} ${DIALECT_H})
endfunction()

add_thorin_dialect(${CMAKE_CURRENT_SOURCE_DIR}/mem/mem.thorin)
add_thorin_dialect(${CMAKE_CURRENT_SOURCE_DIR}/std/std.thorin)
set(DIALECTS    ${DIALECTS}    PARENT_SCOPE) # TODO is there a way around this?
set(DIALECT_MDS ${DIALECT_MDS} PARENT_SCOPE)
