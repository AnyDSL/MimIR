# clear globals
SET(MIM_PLUGIN_LIST   "" CACHE INTERNAL "MIM_PLUGIN_LIST")
SET(MIM_PLUGIN_LAYOUT "" CACHE INTERNAL "MIM_PLUGIN_LAYOUT")

if(NOT MIM_TARGET_NAMESPACE)
    set(MIM_TARGET_NAMESPACE "")
endif()

function(add_mim_plugin)
    set(PLUGIN ${ARGV0})
    cmake_parse_arguments(
        PARSE_ARGV 1        # skip first arg
        PARSED              # prefix of output variables
        "INSTALL"           # options
        ""                  # one-value keywords (none)
        "SOURCES;PRIVATE"   # multi-value keywords
    )

    set(PLUGIN_MIM      ${CMAKE_CURRENT_LIST_DIR}/${PLUGIN}.mim)
    set(OUT_PLUGIN_MIM  ${CMAKE_BINARY_DIR}/lib/mim/${PLUGIN}.mim)
    set(PLUGIN_MD       ${CMAKE_BINARY_DIR}/docs/plug/${PLUGIN}.md)
    set(PLUGIN_D        ${CMAKE_BINARY_DIR}/deps/${PLUGIN}.d)
    set(AUTOGEN_H       ${CMAKE_BINARY_DIR}/include/mim/plug/${PLUGIN}/autogen.h)

    file(
        MAKE_DIRECTORY
            ${CMAKE_BINARY_DIR}/docs/plug/
            ${CMAKE_BINARY_DIR}/deps
            ${CMAKE_BINARY_DIR}/include/mim/plug/${PLUGIN}
    )

    add_custom_command(
        OUTPUT
            ${AUTOGEN_H}
            ${PLUGIN_D}
            ${PLUGIN_MD}
        DEPFILE ${PLUGIN_D}
        COMMAND $<TARGET_FILE:${MIM_TARGET_NAMESPACE}mim> ${PLUGIN_MIM} -P "${CMAKE_CURRENT_LIST_DIR}/.." --bootstrap
            --output-h ${AUTOGEN_H}
            --output-d ${PLUGIN_D}
            --output-md ${PLUGIN_MD}
        MAIN_DEPENDENCY ${PLUGIN_MIM}
        DEPENDS ${MIM_TARGET_NAMESPACE}mim
        COMMENT "Bootstrapping MimIR plugin '${PLUGIN_MIM}'"
        VERBATIM
    )
    add_custom_command(
        OUTPUT  ${OUT_PLUGIN_MIM}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PLUGIN_MIM} ${OUT_PLUGIN_MIM}
        DEPENDS ${PLUGIN_MIM}
        COMMENT "Copy '${PLUGIN_MIM}' to '${OUT_PLUGIN_MIM}'"
    )
    add_custom_target(mim_internal_${PLUGIN}
        DEPENDS
            ${AUTOGEN_H}
            ${PLUGIN_D}
            ${PLUGIN_MD}
            ${OUT_PLUGIN_MIM}
    )

    list(APPEND MIM_PLUGIN_LIST "${PLUGIN}")
    string(APPEND MIM_PLUGIN_LAYOUT "<tab type=\"user\" url=\"@ref ${PLUGIN}\" title=\"${PLUGIN}\"/>")

    # populate to globals
    SET(MIM_PLUGIN_LIST   "${MIM_PLUGIN_LIST}"   CACHE INTERNAL "MIM_PLUGIN_LIST")
    SET(MIM_PLUGIN_LAYOUT "${MIM_PLUGIN_LAYOUT}" CACHE INTERNAL "MIM_PLUGIN_LAYOUT")

    #
    # mim_plugin
    #
    add_library(mim_${PLUGIN} MODULE)
    add_dependencies(mim_${PLUGIN} mim_internal_${PLUGIN})
    target_sources(mim_${PLUGIN}
        PRIVATE
            ${PARSED_SOURCES}
    )
    target_include_directories(mim_${PLUGIN}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> # for autogen.h
    )
    target_link_libraries(mim_${PLUGIN}
        PRIVATE
            ${PARSED_PRIVATE}
            ${MIM_TARGET_NAMESPACE}libmim
    )
    set_target_properties(mim_${PLUGIN}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            PREFIX "lib" # always use "lib" as prefix regardless of OS/compiler
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/mim
    )

    #
    # install
    #
    if(${PARSED_INSTALL})
        install(
            TARGETS
                mim_${PLUGIN}
            EXPORT mim-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/mim
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/mim
            RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/mim
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mim
        )
        install(
            FILES ${CMAKE_BINARY_DIR}/lib/mim/${PLUGIN}.mim
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/mim
        )
        install(
            FILES ${AUTOGEN_H}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mim/plug/${PLUGIN}
        )
    endif()
endfunction()
