set(CLI_HELP   "${CMAKE_CURRENT_BINARY_DIR}/cli-help.sh")
set(XDOT_HELP  "${CMAKE_CURRENT_BINARY_DIR}/xdot-help.gdb")

add_custom_command(
    OUTPUT ${CLI_HELP}
    COMMAND thorin --help > ${CLI_HELP}
    DEPENDS thorin
    COMMENT "Generating help text of Thorin's CLI"
)

add_custom_command(
    OUTPUT ${XDOT_HELP}
    COMMAND gdb --silent -x docs/generate_xdot_help.gdb > ${XDOT_HELP}
    DEPENDS ../scripts/xdot.gdb generate_xdot_help.gdb
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generating GDB xdot help text"
)

set(DOXYFILE        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
set(DOXY_LAYOUT     "${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml")
set(DOXY_INDEX_HTML "${CMAKE_CURRENT_BINARY_DIR}/html/index.html")
set(DOXY_EXTRA_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cli.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/coding.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/dev.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/langref.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/passes.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
)

string(REPLACE ";" " " DOXY_EXTRA_FILES_CONFIG "${DOXY_EXTRA_FILES}")

configure_file(Doxyfile.in ${DOXYFILE} @ONLY)
configure_file(DoxygenLayout.xml.in ${DOXY_LAYOUT} @ONLY)

add_custom_command(
    OUTPUT ${DOXY_INDEX_HTML}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
    MAIN_DEPENDENCY ${DOXYFILE}
    DEPENDS thorin ${CLI_HELP} ${XDOT_HELP} ${DOXY_EXTRA_FILES} ${DOXY_LAYOUT}
    COMMENT "Generating Doxygen HTML documentation"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

list(TRANSFORM THORIN_PLUGIN_LIST PREPEND thorin_internal_ OUTPUT_VARIABLE INTERNAL_LIST)
add_custom_target(docs
    ALL
    DEPENDS
        ${DOXY_INDEX_HTML}
        ${INTERNAL_LIST}
)

add_dependencies(thorin_all docs)
