find_package(Python3 COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

if(MIM_LIT_WITH_VALGRIND)
  set(LIT_VALGRIND_COMMANDLINE "${MEMORYCHECK_COMMAND} ${VALGRIND_COMMAND_OPTIONS} ")
else()
  set(LIT_VALGRIND_COMMANDLINE "")
endif()

find_program(CLANG_EXECUTABLE
             NAMES clang
                   clang-21
                   clang-20
                   clang-19
                   clang-18
                   clang-17
                   clang-16
                   clang-15
                   clang-14
             DOC "clang executable" REQUIRED)
mark_as_advanced(CLANG_EXECUTABLE)

find_program(CLANG_PP_EXECUTABLE
             NAMES clang++
                   clang++-21
                   clang++-20
                   clang++-19
                   clang++-18
                   clang++-17
                   clang++-16
                   clang++-15
                   clang++-14
             DOC "clang++ executable" REQUIRED)
mark_as_advanced(CLANG_PP_EXECUTABLE)

find_program(LLVM_FILECHECK_EXECUTABLE
             NAMES FileCheck
                   FileCheck-21
                   FileCheck-20
                   FileCheck-19
                   FileCheck-18
                   FileCheck-17
                   FileCheck-16
                   FileCheck-15
                   FileCheck-14
             DOC "LLVM FileCheck executable" REQUIRED)
mark_as_advanced(LLVM_FILECHECK_EXECUTABLE)

configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)
add_custom_target(lit
    COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lit" "${CMAKE_CURRENT_BINARY_DIR}" -v --timeout=${MIM_LIT_TIMEOUT}
    DEPENDS mim mim_all_plugins
)
