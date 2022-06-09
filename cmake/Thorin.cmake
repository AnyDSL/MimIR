# clear globals
SET(THORIN_DIALECT_LIST    "" CACHE INTERNAL "THORIN_DIALECT_LIST") 
SET(THORIN_DIALECT_LAYOUT  "" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

function(add_thorin_dialect)
    set(DIALECT ${ARGV0})
    list(SUBLIST ARGV 1 -1 UNPARSED)
    cmake_parse_arguments(
        PARSED              # prefix of output variables
        ""                  # list of names of the boolean arguments (only defined ones will be true)
        "DIALECT"           # list of names of mono-valued arguments
        "SOURCES;DEPENDS"   # list of names of multi-valued arguments (output variables are lists)
        ${UNPARSED}         # arguments of the function to parse, here we take the all original ones
    )

    set(THORIN_FILE     ${CMAKE_CURRENT_SOURCE_DIR}/${DIALECT}/${DIALECT}.thorin)
    set(THORIN_FILE_BIN ${CMAKE_CURRENT_BINARY_DIR}/../lib/thorin/${DIALECT}.thorin)
    set(DIALECT_H       ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.h)
    set(DIALECT_MD      ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.md)

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

    add_library(thorin_${DIALECT} MODULE ${PARSED_SOURCES} ${DIALECT_H})
    set_target_properties(thorin_${DIALECT}
        PROPERTIES 
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
    target_link_libraries(thorin_${DIALECT} libthorin)

    if(PARSED_DEPENDS)
        list(TRANSFORM PARSED_DEPENDS PREPEND thorin_)
        add_dependencies(thorin_${DIALECT} ${PARSED_DEPENDS})
    endif()
endfunction()
