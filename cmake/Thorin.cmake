# clear globals
SET(THORIN_PLUGIN_LIST   "" CACHE INTERNAL "THORIN_PLUGIN_LIST")
SET(THORIN_PLUGIN_LAYOUT "" CACHE INTERNAL "THORIN_PLUGIN_LAYOUT")

if(NOT THORIN_TARGET_NAMESPACE)
    set(THORIN_TARGET_NAMESPACE "")
endif()

function(add_thorin_plugin)
    set(PLUGIN ${ARGV0})
    cmake_parse_arguments(
        PARSE_ARGV 1        # skip first arg
        PARSED              # prefix of output variables
        "INSTALL"           # options
        ""                  # one-value keywords (none)
        "SOURCES;PRIVATE"   # multi-value keywords
    )

    set(PLUGIN_THORIN       ${CMAKE_CURRENT_LIST_DIR}/${PLUGIN}.thorin)
    set(OUT_PLUGIN_THORIN   ${CMAKE_BINARY_DIR}/lib/thorin/${PLUGIN}.thorin)
    set(PLUGIN_MD           ${CMAKE_BINARY_DIR}/docs/plug/${PLUGIN}.md)
    set(PLUGIN_D            ${CMAKE_BINARY_DIR}/deps/${PLUGIN}.d)
    set(AUTOGEN_H           ${CMAKE_BINARY_DIR}/include/thorin/plug/${PLUGIN}/autogen.h)

    file(
        MAKE_DIRECTORY
            ${CMAKE_BINARY_DIR}/docs/plug/
            ${CMAKE_BINARY_DIR}/deps
            ${CMAKE_BINARY_DIR}/include/thorin/plug/${PLUGIN}
    )

    add_custom_command(
        OUTPUT
            ${AUTOGEN_H}
            ${PLUGIN_D}
            ${PLUGIN_MD}
        DEPFILE ${PLUGIN_D}
        COMMAND $<TARGET_FILE:${THORIN_TARGET_NAMESPACE}thorin> ${PLUGIN_THORIN} -P "${CMAKE_CURRENT_LIST_DIR}/.." --bootstrap --output-h ${AUTOGEN_H} --output-d ${PLUGIN_D} --output-md ${PLUGIN_MD}
        MAIN_DEPENDENCY ${PLUGIN_THORIN}
        DEPENDS ${THORIN_TARGET_NAMESPACE}thorin
        COMMENT "Bootstrapping Thorin '${PLUGIN_THORIN}'"
        VERBATIM
    )
    add_custom_command(
        OUTPUT  ${OUT_PLUGIN_THORIN}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PLUGIN_THORIN} ${OUT_PLUGIN_THORIN}
        DEPENDS ${PLUGIN_THORIN}
        COMMENT "Copy '${PLUGIN_THORIN}' to '${OUT_PLUGIN_THORIN}'"
    )
    add_custom_target(thorin_internal_${PLUGIN}
        DEPENDS
            ${AUTOGEN_H}
            ${PLUGIN_D}
            ${PLUGIN_MD}
            ${OUT_PLUGIN_THORIN}
    )

    list(APPEND THORIN_PLUGIN_LIST "${PLUGIN}")
    string(APPEND THORIN_PLUGIN_LAYOUT "<tab type=\"user\" url=\"@ref ${PLUGIN}\" title=\"${PLUGIN}\"/>")

    # populate to globals
    SET(THORIN_PLUGIN_LIST   "${THORIN_PLUGIN_LIST}"   CACHE INTERNAL "THORIN_PLUGIN_LIST")
    SET(THORIN_PLUGIN_LAYOUT "${THORIN_PLUGIN_LAYOUT}" CACHE INTERNAL "THORIN_PLUGIN_LAYOUT")

    #
    # thorin_plugin
    #
    add_library(thorin_${PLUGIN} MODULE)
    add_dependencies(thorin_${PLUGIN} thorin_internal_${PLUGIN})
    target_sources(thorin_${PLUGIN}
        PRIVATE
            ${PARSED_SOURCES}
    )
    target_include_directories(thorin_${PLUGIN}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> # for autogen.h
    )
    target_link_libraries(thorin_${PLUGIN}
        PRIVATE
            ${PARSED_PRIVATE}
            ${THORIN_TARGET_NAMESPACE}libthorin
    )
    set_target_properties(thorin_${PLUGIN}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            PREFIX "lib" # always use "lib" as prefix regardless of OS/compiler
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/thorin
    )

    #
    # install
    #
    if(${PARSED_INSTALL})
        install(
            TARGETS
                thorin_${PLUGIN}
            EXPORT thorin-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/thorin
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/thorin
            RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/thorin
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
        install(
            FILES ${CMAKE_BINARY_DIR}/lib/thorin/${PLUGIN}.thorin
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/thorin
        )
        install(
            FILES ${AUTOGEN_H}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/plug/${PLUGIN}
        )
    endif()
endfunction()
