find_package(Python3 COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(check
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v
        DEPENDS thorin thorin_affine thorin_core thorin_mem direct)

# We don't want to test python for memory leaks.. :/
# add_test(NAME lit COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v)
