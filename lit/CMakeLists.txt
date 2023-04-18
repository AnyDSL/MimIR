find_package(Python3 COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

if(THORIN_LIT_WITH_VALGRIND)
  set(LIT_VALGRIND_COMMANDLINE "${MEMORYCHECK_COMMAND} ${VALGRIND_COMMAND_OPTIONS} ")
else()
  set(LIT_VALGRIND_COMMANDLINE "")
endif()

configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(lit
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v --timeout=300
        DEPENDS thorin thorin_all_plugins)

# We don't want to test python for memory leaks.. :/
# add_test(NAME lit COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v)
