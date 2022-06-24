# clear globals
SET(THORIN_DIALECT_LIST    "" CACHE INTERNAL "THORIN_DIALECT_LIST") 
SET(THORIN_DIALECT_LAYOUT  "" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

if(NOT THORIN_TARGET_NAMESPACE)
    set(THORIN_TARGET_NAMESPACE "")
endif()

function(add_thorin_dialect)
    set(DIALECT ${ARGV0})
    list(SUBLIST ARGV 1 -1 UNPARSED)
    cmake_parse_arguments(
        PARSED              # prefix of output variables
        "INSTALL"           # list of names of the boolean arguments (only defined ones will be true)
        "DIALECT"           # list of names of mono-valued arguments
        "SOURCES;DEPENDS"   # list of names of multi-valued arguments (output variables are lists)
        ${UNPARSED}         # arguments of the function to parse, here we take the all original ones
    )

    set(THORIN_LIB_DIR ${CMAKE_BINARY_DIR}/lib/thorin)

    list(TRANSFORM PARSED_DEPENDS       PREPEND ${THORIN_LIB_DIR}/ OUTPUT_VARIABLE DEPENDS_THORIN_FILES)
    list(TRANSFORM DEPENDS_THORIN_FILES  APPEND .thorin)
    list(TRANSFORM PARSED_DEPENDS       PREPEND ${CMAKE_CURRENT_BINARY_DIR}/ OUTPUT_VARIABLE DEPENDS_HEADER_FILES)
    list(TRANSFORM DEPENDS_HEADER_FILES  APPEND .h)

    set(THORIN_FILE     ${CMAKE_CURRENT_SOURCE_DIR}/${DIALECT}/${DIALECT}.thorin)
    set(THORIN_FILE_LIB_DIR ${THORIN_LIB_DIR}/${DIALECT}.thorin)
    set(DIALECT_H       ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.h)
    set(DIALECT_MD      ${CMAKE_CURRENT_BINARY_DIR}/${DIALECT}.md)

    list(APPEND THORIN_DIALECT_LIST "${DIALECT}")
    string(APPEND THORIN_DIALECT_LAYOUT "<tab type=\"user\" url=\"@ref ${DIALECT}\" title=\"${DIALECT}\"/>")

    # populate to globals
    SET(THORIN_DIALECT_LIST   "${THORIN_DIALECT_LIST}"   CACHE INTERNAL "THORIN_DIALECT_LIST")
    SET(THORIN_DIALECT_LAYOUT "${THORIN_DIALECT_LAYOUT}" CACHE INTERNAL "THORIN_DIALECT_LAYOUT")

    # copy dialect thorin file to lib/thorin/${DIALECT}.thorin
    add_custom_command(
        OUTPUT  ${THORIN_FILE_LIB_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${THORIN_FILE} ${THORIN_FILE_LIB_DIR}
        DEPENDS ${THORIN_FILE} ${DEPENDS_THORIN_FILES}
    )

    add_custom_command(
        OUTPUT ${DIALECT_MD} ${DIALECT_H}
        COMMAND $<TARGET_FILE:${THORIN_TARGET_NAMESPACE}thorin> -e md -e h ${THORIN_FILE_LIB_DIR} -D ${THORIN_LIB_DIR}
        DEPENDS ${THORIN_TARGET_NAMESPACE}thorin ${THORIN_FILE_LIB_DIR}
        COMMENT "Bootstrapping Thorin dialect '${DIALECT}' from '${THORIN_FILE}'"
    )
    add_custom_target(${DIALECT} ALL DEPENDS ${DIALECT_MD} ${DIALECT_H})

    add_library(thorin_${DIALECT} 
        MODULE 
            ${PARSED_SOURCES}       # original sources passed to add_thorin_dialect
            ${DIALECT_H}            # the generated header of this dialect
            ${DEPENDS_HEADER_FILES} # the generated headers of the dialects we depend on
    )

    set_target_properties(thorin_${DIALECT}
        PROPERTIES 
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            LIBRARY_OUTPUT_DIRECTORY ${THORIN_LIB_DIR}
    )

    target_link_libraries(thorin_${DIALECT} ${THORIN_TARGET_NAMESPACE}libthorin)

    if(${PARSED_INSTALL})
        install(TARGETS thorin_${DIALECT} EXPORT install_exports LIBRARY DESTINATION lib/thorin RUNTIME DESTINATION lib/thorin INCLUDES DESTINATION include)
        install(FILES ${THORIN_FILE_LIB_DIR} DESTINATION lib/thorin)
        install(FILES ${DIALECT_H} DESTINATION include/dialects)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${DIALECT} DESTINATION include/dialects FILES_MATCHING PATTERN *.h)
    endif()
endfunction()
