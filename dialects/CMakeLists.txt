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
target_link_libraries(thorin_foo libthorin)

add_thorin_dialect(std)
add_thorin_dialect(mem)
