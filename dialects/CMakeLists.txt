add_library(thorin_foo MODULE
    foo.cpp
    foo.h
)

set_target_properties(thorin_foo
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

target_include_directories(libthorin PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) # for thorin_foo_export.h
target_link_libraries(thorin_foo libthorin)
