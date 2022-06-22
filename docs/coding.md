# Coding & Debugging {#coding}

[TOC]

This document comprises some information that is related to coding but does not directly related to the API.

## Coding Style

Use the following coding conventions:
* class/type names in `CamelCase`
* constants as defined in an `enum` or via `static const` in `Camel_Snake_Case`
* macro names in `SNAKE_IN_ALL_CAPS`
* everything else like variables, functions, etc. in `snake_case`
* use a trailing underscore suffix for a `private_or_protected_member_variable_`
* don't do that for a `public_member_variable`
* use `struct` for [plain old data](https://en.cppreference.com/w/cpp/named_req/PODType)
* use `class` for everything else
* visibility groups in this order:
    1. `public`
    2. `protected`
    3. `private`
* prefer `// C++-style comments` over `/* C-style comments */`
* use `/// three slashes for Doxygen` and [group](https://www.doxygen.nl/manual/grouping.html) your methods into logical units if possible

For all the other minute details like indentation width etc. use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and the provided `.clang-format` file in the root of the repository.
In order to run `clang-format` automatically on all changed files, switch to the provided pre-commit hook:
```sh
git config --local core.hooksPath .githooks/
```
Note that you can [disable clang-format for a piece of code](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code).
In addition, you might want to check out plugins like the [Vim integration](https://clang.llvm.org/docs/ClangFormat.html#vim-integration).

# Debugging

For logging and automatic firing of breakpoints refer to the [Command-Line Reference](@ref clidebug).

## Conditional Breakpoints

Often, you will want to inspect a certain thorin::Def at a particular point within the program.
You can use [conditional breakpoints](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_33.html) for this.
For example, the following GDB command will break, if the thorin::Def::gid of variable `def` is `666` in source code location `foo.cpp:23`:
```gdb
break foo.cpp:23 if def->gid() == 666
```

## Catching Throw

For several things like errors in Thorin's front end, Thorin relies on C++ exceptions for error handling.
Simply, do this to encounter them within [GDB](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_30.html):
```gdb
catch throw
```

## Valgrind & GDB

If you encounter memory related problems, you might want to run the program with [Valgrind's GDB server](https://valgrind.org/docs/manual/manual-core-adv.html).
Simply launch the program like this
```sh
valgrind --vgdb=yes --vgdb-error=0 thorin-gtest
```
and follow the instructions.

## GoogleTest

Thorin's unit test suite uses [GoogleTest](https://google.github.io/googletest/).
During debugging you probably only want to run a specifig test case.
You can [filter](https://github.com/google/googletest/blob/main/docs/advanced.md#running-a-subset-of-the-tests) the test cases like this:
```sh
./thorin-gtest --gtest_filter="*Loc*"
```
This command lists all available tests:
```sh
./thorin-gtest --gtest_list_tests
```
In addition, you may find it helpful to turn assertion failures into debugger breakpoints:
```sh
./thorin-test --gtest_break_on_failure
```

## VS Code

As a utility to make debugging Thorin itself less painful with certain debuggers, the `thorin.natvis` file can be loaded for getting more expressive value inspection.
In VS Code you can do so by adding the following to the `launch.json` configurations. When launching from VS Code via CMake, put it in `settings.json`'s `"cmake.debugConfig":`:
```json
"visualizerFile": "${workspaceFolder}/thorin.natvis",
"showDisplayString": true,
```
