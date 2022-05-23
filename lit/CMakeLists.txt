configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(check
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v
        DEPENDS thorin thorin_affine)

add_test(NAME lit COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v)
