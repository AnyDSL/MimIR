find_package(Python3 COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

if(THORIN_LIT_WITH_VALGRIND)
  set(LIT_VALGRIND_COMMANDLINE "${MEMORYCHECK_COMMAND} ${VALGRIND_COMMAND_OPTIONS} ")
else()
  set(LIT_VALGRIND_COMMANDLINE "")
endif()

configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(check
<<<<<<< HEAD
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v
        DEPENDS thorin thorin_affine thorin_autodiff thorin_clos thorin_compile thorin_core thorin_demo thorin_direct thorin_math thorin_matrix thorin_mem thorin_refly)
=======
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v --timeout=300
        DEPENDS thorin thorin_affine thorin_compile thorin_demo thorin_direct thorin_autodiff thorin_clos thorin_core thorin_math thorin_mem thorin_refly)
>>>>>>> origin/master

# We don't want to test python for memory leaks.. :/
# add_test(NAME lit COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v)
