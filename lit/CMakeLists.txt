configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(check
        COMMAND lit "${CMAKE_CURRENT_BINARY_DIR}" -v)

add_test(NAME lit COMMAND lit "${CMAKE_CURRENT_BINARY_DIR}" -v)
