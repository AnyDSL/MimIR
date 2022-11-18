# Coding & Debugging {#coding}

[TOC]

This document comprises some information that is related to coding but not directly to the API.

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
* use [Markdown-style](https://doxygen.nl/manual/markdown.html) Doxygen comments
* methods/functions that return a `bool` should be prefixed with `is_`
* methods/functions that return a `std::optional` or a pointer that may be `nullptr` should be prefixed with `isa_`

For all the other minute details like indentation width etc. use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and the provided `.clang-format` file in the root of the repository.
In order to run `clang-format` automatically on all changed files, switch to the provided pre-commit hook:
```sh
git config --local core.hooksPath .githooks/
```
Note that you can [disable clang-format for a piece of code](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code).
In addition, you might want to check out plugins like the [Vim integration](https://clang.llvm.org/docs/ClangFormat.html#vim-integration).

## Debugging

For logging and automatic firing of breakpoints refer to the [Command-Line Reference](@ref clidebug).

### Dumping

Note that you can simply invoke thorin::Def::dump, thorin::Def::write, thorin::World::dump, or thorin::World::write from within [GDB](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_30.html):
```gdb
(gdb) call def->dump()
(gdb) call def->dump(0)
(gdb) call def->dump(3)
(gdb) call world().write("out.thorin")
```
In particular, note the different output levels of thorin::Def::dump.
What is more, you can adjust the output behavior directly from within GDB by modifying thorin::World::flags or thorin::World::log:
```gdb
(gdb) call world.flags().dump_gid = 1
(gdb) call world.flags().dump_recursive = 1
(gdb) call world.log().level = 4
```
Another useful feature is to retrieve a `Def*` from a thorin::Def::gid via thorin::World::gid2def:
```gdb
(gdb) p world.gid2def(123);
$1 = ...
(gdb) $1->dump();
```

### Conditional Breakpoints

Often, you will want to inspect a certain thorin::Def at a particular point within the program.
You can use [conditional breakpoints](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_33.html) for this.
For example, the following GDB command will break, if the thorin::Def::gid of variable `def` is `42` in source code location `foo.cpp:23`:
```gdb
break foo.cpp:23 if def->gid() == 42
```

### Catching Throw

For several things like errors in Thorin's front end, Thorin relies on C++ exceptions for error handling.
Simply, do this to encounter them within GDB:
```gdb
catch throw
```

### Valgrind & GDB

If you encounter memory related problems, you might want to run the program with [Valgrind's GDB server](https://valgrind.org/docs/manual/manual-core-adv.html).
Simply launch the program like this
```sh
valgrind --vgdb=yes --vgdb-error=0 thorin-gtest
```
and follow the instructions.

### VS Code

As a utility to make debugging Thorin itself less painful with certain debuggers, the `thorin.natvis` file can be loaded for getting more expressive value inspection.
In VS Code you can do so by adding the following to the `launch.json` configurations. When launching from VS Code via CMake, put it in `settings.json`'s `"cmake.debugConfig":`:
```json
"visualizerFile": "${workspaceFolder}/thorin.natvis",
"showDisplayString": true,
```

## Tests {#tests}

### lit Tests

Run the [lit](https://llvm.org/docs/CommandGuide/lit.html) testsuite with:
```sh
cmake --build build -t check
```
You can manually invoke the lit tests like this and maybe filter for a specific test:
```sh
cd lit
./lit ../build/lit -a --filter foo.thorin
```

### GoogleTest

Run the [GoogleTest](https://google.github.io/googletest/) unit tests within the `build` folder with:
```sh
ctest
```
In addition, you can enable [Valgrind](https://valgrind.org/) with:
```sh
ctest -T memcheck
```

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

## Syntax Highlighting

[This](https://github.com/AnyDSL/vim-thorin2) Vim plugin provides syntax highlighting for Thorin files.

## Third-Party Dialects

After installing Thorin, third-party dialects just need to find the `thorin` package:
```cmake
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(dialect)

find_package(thorin)

add_thorin_dialect(dialect
    SOURCES
        dialect/dialect.h
        dialect/dialect.cpp
)
```
Use
```cmake
cmake .. -Dthorin_DIR=<THORIN_INSTALL_PREFIX>/lib/cmake/thorin
```
to configure the project.

Check out the [demo](@ref demo) dialect for a minimalistic plugin.

### add_thorin_dialect

Registers a new Thorin dialect.

```
add_thorin_dialect(<name>
    [SOURCES <source>...]
    [DEPENDS <other_dialect_name>...]
    [HEADER_DEPENDS <other_dialect_name>...]
    [INSTALL])
```

The `<name>` is expected to be the name of the dialect. This means, there
should be (relative to your CMakeLists.txt) a file `<name>/<name>.thorin`
containing the axiom declarations.

- `SOURCES`: The values to the `SOURCES` argument are the source files used
    to build the loadable plugin containing normalizers, passes and backends.
    One of the source files must export the `thorin_get_dialect_info` function.
    `add_thorin_dialect` creates a new target called `thorin_<name>` that builds
    the dialect plugin.
    Custom properties can be specified in the using `CMakeLists.txt` file,
    e.g. adding include paths is done with `target_include_directories(thorin_<name> <path>..)`.
- `DEPENDS`: The `DEPENDS` arguments specify the relation between multiple
    dialects. This makes sure that the bootstrapping of the dialect is done
    whenever a depended-upon dialect description is changed.
    E.g. `core` depends on `mem`, therefore whenever `mem.thorin` changes,
    `core.thorin` has to be bootstrapped again as well.
- `HEADER_DEPENDS`: The `HEADER_DEPENDS` arguments specify dependencies
    of a dialect's plugin on the generated header of another dialect.
    E.g. `mem.thorin` does not import `core.thorin` but the plugin relies
    on the `%core.conv` axiom. Therefore `mem` requires `core`'s autogenerated
    header to be up-to-date.
- `INSTALL`: Specify, if the dialect description, plugin and headers shall
    be installed with `make install`.
    To export the targets, the export name `install_exports` has to be
    exported accordingly (see [install(EXPORT ..)](https://cmake.org/cmake/help/latest/command/install.html#export))
