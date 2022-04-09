add_executable(thorin
    main.cpp
    dialects.cpp
    dialects.h
    md_converter.cpp
    md_converter.h)

target_link_libraries(thorin libthorin ${CMAKE_DL_LIBS} lyra)
