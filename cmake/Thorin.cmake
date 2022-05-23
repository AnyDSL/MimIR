# clear globals
SET(THORIN_DIALECT_LIST    "" CACHE INTERNAL "THORIN_DIALECT_LIST") 
SET(THORIN_DIALECT_LAYOUT  "" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

add_custom_target(copy_dialects ALL)

function(add_thorin_dialect DIALECT)
    set(THORIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${DIALECT}/${DIALECT}.thorin)
    set(THORIN_FILE_BIN ${CMAKE_CURRENT_BINARY_DIR}/../lib/thorin/${DIALECT}.thorin)
    set(DIALECT_H  ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.h)
    set(DIALECT_MD ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.md)

    list(APPEND THORIN_DIALECT_LIST "${DIALECT}")
    string(APPEND THORIN_DIALECT_LAYOUT "<tab type=\"user\" url=\"@ref ${DIALECT}\" title=\"${DIALECT}\"/>")

    # populate to globals
    SET(THORIN_DIALECT_LIST   "${THORIN_DIALECT_LIST}"   CACHE INTERNAL "THORIN_DIALECT_LIST")
    SET(THORIN_DIALECT_LAYOUT "${THORIN_DIALECT_LAYOUT}" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

    add_custom_target(
        copy_${DIALECT}
        COMMAND ${CMAKE_COMMAND} -E copy ${THORIN_FILE} ${THORIN_FILE_BIN}
    )
    
    # make sure all dialect files are copied
    add_dependencies(copy_dialects copy_${DIALECT})

    add_custom_command(
        OUTPUT ${DIALECT_MD} ${DIALECT_H}
        COMMAND thorin -e md -e h ${THORIN_FILE_BIN}
        DEPENDS thorin copy_dialects
        COMMENT "Bootstrapping Thorin dialect '${DIALECT}' from '${THORIN_FILE}'"
    )
    add_custom_target(${DIALECT} ALL DEPENDS ${DIALECT_MD} ${DIALECT_H})
endfunction()
