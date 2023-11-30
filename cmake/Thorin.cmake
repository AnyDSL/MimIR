# clear globals
SET(THORIN_PLUGIN_LIST   "" CACHE INTERNAL "THORIN_PLUGIN_LIST")
SET(THORIN_PLUGIN_LAYOUT "" CACHE INTERNAL "THORIN_PLUGIN_LAYOUT")

if(NOT THORIN_TARGET_NAMESPACE)
    set(THORIN_TARGET_NAMESPACE "")
endif()

#[=======================================================================[
add_thorin_plugin
-------------------

Registers a new Thorin plugin.

```
add_thorin_plugin(<plugin_name>
    [SOURCES <source>...]
    [HEADERS <header>...]
    [PRIVATE <private_item>...]
    [INTERFACE <interface_item>...]
    [PUBLIC <public_item>...]
    [INSTALL])
```

The `<plugin_name>` is expected to be the name of the plugin. This means, there
should be (relative to your CMakeLists.txt) a file `<plugin_name>.thorin`
containing the axiom declarations. This will generate a header
`plug/<plugin_name>/autogen.h` that can be used in normalizers and passes to
identify the axioms. The command will create three targets:

1. `thorin_internal_<plugin_name>`: This is an internal target to bootstrap the
    plugin, i.e. generate the header etc.
2. `thorin_interface_<plugin_name>`: This is a header-only `INTERFACE` library
    that consists of all `<header>` files. It "links" via `INTERFACE` with all
    `<interface_item>` targets. This means everybody who links to
    `thorin_interface_<plugin_name>` will also have access to the include
    directories of these targets. In addition, `thorin_interface_<plugin_name>`
    depends on `thorin_internal_<plugin_name>`.
3. `thorin_<plugin_name>`: This is the actual `MODULE` library and conists of
    all `<source>` files and links `PUBLIC` to `thorin_interface_<plugin_name>`.
    Thus, all sources will have access to the headers of
    `thorin_interface_<plugin_name>` and to all other targets specified via
    `INTERFACE`. Furthermore, you can specify additional `public_item` targets that
    the module should link against as `PUBLIC`. Finally, you can specify additional
    `<private_item>` build dependencies.

The `<source>` files are used to build the loadable plugin containing
normalizers, passes and backends. One of the source files must export the
`thorin_get_plugin` function. Custom properties can be specified in the
`CMakeLists.txt` file, e.g. adding include paths is done with
`target_include_directories(thorin_<plugin_name> <path>...)`.

- `INSTALL`: Specify, if the plugin description, plugin and headers shall
    be installed with `make install`.
    To export the targets, the export name `thorin-targets` has to be
    exported accordingly (see
    [install(EXPORT ..)](https://cmake.org/cmake/help/latest/command/install.html#export))


## Note: a copy of this text is in `docs/coding.md`. Please update!
#]=======================================================================]
function(add_thorin_plugin)
    set(PLUGIN ${ARGV0})
    list(SUBLIST ARGV 1 -1 UNPARSED)
    cmake_parse_arguments(
        PARSED                                      # prefix of output variables
        "INSTALL"                                   # list of names of the boolean arguments (only defined ones will be true)
        "THORIN"                                    # list of names of mono-valued arguments
        "HEADERS;SOURCES;PUBLIC;PRIVATE;INTERFACE"  # list of names of multi-valued arguments (output variables are lists)
        ${UNPARSED}                                 # arguments of the function to parse, here we take the all original ones
    )

    list(TRANSFORM PARSED_INTERFACES PREPEND thorin_interface_ OUTPUT_VARIABLE INTERFACES)

    set(INCLUDE_DIR_PLUG ${CMAKE_BINARY_DIR}/include/thorin/plug/${PLUGIN})
    set(LIB_DIR_PLUG     ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/thorin)

    set(PLUGIN_THORIN ${CMAKE_CURRENT_LIST_DIR}/${PLUGIN}.thorin)
    set(PLUGIN_MD     ${CMAKE_BINARY_DIR}/docs/plug/${PLUGIN}.md)
    set(PLUGIN_D      ${CMAKE_BINARY_DIR}/deps/${PLUGIN}.d)
    set(AUTOGEN_H     ${INCLUDE_DIR_PLUG}/autogen.h)

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
        OUTPUT  ${LIB_DIR_PLUG}/${PLUGIN}.thorin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PLUGIN_THORIN} ${LIB_DIR_PLUG}/${PLUGIN}.thorin
        DEPENDS ${PLUGIN_THORIN}
        COMMENT "Copy '${PLUGIN_THORIN}' to '${LIB_DIR_PLUG}.${PLUGIN}.thorin'"
    )
    add_custom_target(thorin_internal_${PLUGIN}
        DEPENDS
            ${AUTOGEN_H}
            ${PLUGIN_D}
            ${PLUGIN_MD}
            ${LIB_DIR_PLUG}/${PLUGIN}.thorin
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
    add_library(thorin_${PLUGIN} MODULE)
    target_sources(thorin_${PLUGIN}
        PRIVATE
            ${PARSED_SOURCES}
            ${PARSED_PRIVATE}
    )
    target_link_libraries(thorin_${PLUGIN}
        PUBLIC
            thorin_interface_${PLUGIN}
            ${PARSED_PUBLIC}
    )
    set_target_properties(thorin_${PLUGIN}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN 1
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
            PREFIX "lib" # always use "lib" as prefix regardless of OS/compiler
            LIBRARY_OUTPUT_DIRECTORY ${LIB_DIR_PLUG}
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
