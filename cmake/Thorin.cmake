# clear globals
SET(THORIN_PLUGIN_LIST   "" CACHE INTERNAL "THORIN_PLUGIN_LIST")
SET(THORIN_PLUGIN_LAYOUT "" CACHE INTERNAL "THORIN_PLUGIN_LAYOUT")

if(NOT THORIN_TARGET_NAMESPACE)
    set(THORIN_TARGET_NAMESPACE "")
endif()

function(add_thorin_plugin)
    set(PLUGIN ${ARGV0})
    cmake_parse_arguments(
        PARSE_ARGV 1                                # skip first arg
        PARSED                                      # prefix of output variables
        "SHARED;INSTALL"                            # options
        ""                                          # one-value keywords (none)
        "HEADERS;SOURCES;PUBLIC;PRIVATE;INTERFACE"  # multi-value keywords
    )

    list(TRANSFORM PARSED_INTERFACES PREPEND thorin_interface_ OUTPUT_VARIABLE INTERFACES)

    set(PLUGIN_THORIN       ${CMAKE_CURRENT_LIST_DIR}/${PLUGIN}.thorin)
    set(OUT_PLUGIN_THORIN   ${THORIN_LIBRARY_OUTPUT_DIRECTORY}/${PLUGIN}.thorin)
    set(INCLUDE_DIR_PLUG    ${CMAKE_BINARY_DIR}/include/thorin/plug/${PLUGIN})
    set(PLUGIN_MD           ${CMAKE_BINARY_DIR}/docs/plug/${PLUGIN}.md)
    set(PLUGIN_D            ${CMAKE_BINARY_DIR}/deps/${PLUGIN}.d)
    set(AUTOGEN_H           ${INCLUDE_DIR_PLUG}/autogen.h)

    file(MAKE_DIRECTORY ${INCLUDE_DIR_PLUG})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/deps)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/docs/plug/)

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
    # thorin_interface_plugin
    #
    add_library(thorin_interface_${PLUGIN} INTERFACE)
    add_dependencies(thorin_interface_${PLUGIN} thorin_internal_${PLUGIN})
    target_sources(thorin_interface_${PLUGIN}
        INTERFACE
            FILE_SET thorin_headers_${PLUGIN}
            TYPE HEADERS
            BASE_DIRS
                ${CMAKE_CURRENT_LIST_DIR}/include
                ${CMAKE_BINARY_DIR}/include
            FILES
                ${AUTOGEN_H}        # the generated header of this plugin
                ${PARSED_HEADERS}   # original headers passed to add_thorin_plugin
    )
    target_link_libraries(thorin_interface_${PLUGIN}
        INTERFACE
            ${PARSED_INTERFACE}
            libthorin
    )

    #
    # thorin_plugin
    #
    if(${PARSED_SHARED})
        add_library(thorin_${PLUGIN} SHARED)
    else()
        add_library(thorin_${PLUGIN} MODULE)
    endif()
    target_sources(thorin_${PLUGIN}
        PRIVATE
            ${PARSED_SOURCES}
    )
    target_link_libraries(thorin_${PLUGIN}
        PUBLIC
            thorin_interface_${PLUGIN}
            ${PARSED_PUBLIC}
        PRIVATE
            ${PARSED_PRIVATE}
    )
    set_target_properties(thorin_${PLUGIN}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            PREFIX "lib"                                                # always use "lib" as prefix regardless of OS/compiler
            LIBRARY_OUTPUT_DIRECTORY ${THORIN_LIBRARY_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY ${THORIN_LIBRARY_OUTPUT_DIRECTORY} # place for a dll in a SHARED thorin plugin
    )

    #
    # install
    #
    if(${PARSED_INSTALL})
        install(
            TARGETS
                thorin_interface_${PLUGIN}
                thorin_${PLUGIN}
            EXPORT thorin-targets
            FILE_SET thorin_headers_${PLUGIN}
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/thorin"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/thorin"
            RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/thorin"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        )
        install(
            FILES ${LIB_DIR_PLUGIN_THORIN}
            DESTINATION lib/thorin
        )
        install(
            FILES ${AUTOGEN_H}
            DESTINATION "include/plug/${PLUGIN}"
        )
    endif()
    if(TARGET thorin_all_plugins)
        add_dependencies(thorin_all_plugins thorin_${PLUGIN})
    endif()
endfunction()
