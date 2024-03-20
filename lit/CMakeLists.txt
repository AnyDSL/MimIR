find_package(Python3 COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

if(THORIN_LIT_WITH_VALGRIND)
  set(LIT_VALGRIND_COMMANDLINE "${MEMORYCHECK_COMMAND} ${VALGRIND_COMMAND_OPTIONS} ")
else()
  set(LIT_VALGRIND_COMMANDLINE "")
endif()

configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(lit
    COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v --timeout=${THORIN_LIT_WITH_VALGRIND}
    DEPENDS thorin thorin_all_plugins
)
