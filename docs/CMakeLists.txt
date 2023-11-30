set(CLI_HELP "${CMAKE_CURRENT_BINARY_DIR}/cli-help.sh")

add_custom_command(
    OUTPUT ${CLI_HELP}
    COMMAND thorin --help > ${CLI_HELP}
    DEPENDS thorin
    COMMENT "Generating help text of Thorin's CLI"
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
list(TRANSFORM THORIN_PLUGIN_LIST PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/" OUTPUT_VARIABLE STRIP_FROM_INC_PATH)
list(TRANSFORM STRIP_FROM_INC_PATH APPEND /include)
string(REPLACE ";" " " STRIP_FROM_INC_PATH "${STRIP_FROM_INC_PATH}")
cmake_print_variables(STRIP_FROM_INC_PATH)
configure_file(Doxyfile.in ${DOXYFILE} @ONLY)
configure_file(DoxygenLayout.xml.in ${DOXY_LAYOUT} @ONLY)

add_custom_command(
    OUTPUT ${DOXY_INDEX_HTML}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
    MAIN_DEPENDENCY ${DOXYFILE}
    DEPENDS thorin ${CLI_HELP} ${DOXY_EXTRA_FILES} ${DOXY_LAYOUT}
    COMMENT "Generating Doxygen HTML documentation"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

list(TRANSFORM THORIN_PLUGIN_LIST PREPEND thorin_interface_ OUTPUT_VARIABLE INTERFACE_LIST)
add_custom_target(docs
    ALL
    DEPENDS
        ${DOXY_INDEX_HTML}
        ${INTERFACE_LIST}
)
