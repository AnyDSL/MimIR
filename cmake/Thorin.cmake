# clear globals
SET(THORIN_DIALECT_LIST    "" CACHE INTERNAL "THORIN_DIALECT_LIST") 
SET(THORIN_DIALECT_LAYOUT  "" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

function(add_thorin_dialect DIALECT)
    set(THORIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${DIALECT}/${DIALECT}.thorin)
    set(THORIN_FILE_BIN ${CMAKE_CURRENT_BINARY_DIR}/../lib/thorin/${DIALECT}.thorin)
    set(DIALECT_H  ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.h)
    set(DIALECT_MD ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.md)

    list(LENGTH THORIN_DIALECT_LIST NUM_DIALECTS)
    list(APPEND THORIN_DIALECT_LIST "${DIALECT}")
    string(APPEND THORIN_DIALECT_LAYOUT "<tab type=\"user\" url=\"@ref ${DIALECT}\" title=\"${DIALECT}\"/>")

    # populate to globals
    SET(THORIN_DIALECT_LIST   "${THORIN_DIALECT_LIST}"   CACHE INTERNAL "THORIN_DIALECT_LIST")
    SET(THORIN_DIALECT_LAYOUT "${THORIN_DIALECT_LAYOUT}" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

    # copy dialect thorin file to lib/thorin/${DIALECT}.thorin
    add_custom_command(OUTPUT ${THORIN_FILE_BIN}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${THORIN_FILE} ${THORIN_FILE_BIN}
        DEPENDS ${THORIN_FILE}
    )
    # establish dependency tree - all dialects can depend on all other dialects via .import
    if(NUM_DIALECTS GREATER 0)
        add_custom_command(OUTPUT copy_dialects APPEND
            DEPENDS ${THORIN_FILE_BIN}
        )
    else()
        add_custom_command(OUTPUT copy_dialects
            DEPENDS ${THORIN_FILE_BIN}
        )
        set_source_files_properties(copy_dialects PROPERTIES SYMBOLIC TRUE)
    endif()
    
    add_custom_command(
        OUTPUT ${DIALECT_MD} ${DIALECT_H}
        COMMAND thorin -e md -e h ${THORIN_FILE_BIN} -D ${CMAKE_CURRENT_BINARY_DIR}/../lib/thorin/
        DEPENDS thorin copy_dialects
        COMMENT "Bootstrapping Thorin dialect '${DIALECT}' from '${THORIN_FILE}'"
    )
    add_custom_target(${DIALECT} ALL DEPENDS ${DIALECT_MD} ${DIALECT_H})
endfunction()
