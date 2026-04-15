# Coding & Debugging {#coding}

[TOC]

This page collects information that is useful while working on MimIR itself, but is not directly part of the API.

## Build Options {#build_options}

If you do not have a [GitHub account set up with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), you can clone MimIR via HTTPS instead:

```sh
git clone --recursive https://github.com/AnyDSL/MimIR.git
```

The following CMake switches are available:

| CMake Switch            | Options                                  | Default      | Comment                                                                                         |
| ----------------------- | ---------------------------------------- | ------------ | ----------------------------------------------------------------------------------------------- |
| `CMAKE_BUILD_TYPE`      | `Debug` \| `Release` \| `RelWithDebInfo` | `Debug`      | Build type.                                                                                     |
| `CMAKE_INSTALL_PREFIX`  |                                          | `/usr/local` | Install prefix.                                                                                 |
| `MIM_BUILD_DOCS`        | `ON` \| `OFF`                            | `OFF`        | If `ON`, build the documentation <br> (requires Doxygen).                                       |
| `MIM_BUILD_EXAMPLES`    | `ON` \| `OFF`                            | `OFF`        | If `ON`, build the examples.                                                                    |
| `MIM_ENABLE_CHECKS`     | `ON` \| `OFF`                            | `ON`         | If `ON`, enable expensive runtime checks <br> (requires `CMAKE_BUILD_TYPE=Debug`).              |
| `BUILD_TESTING`         | `ON` \| `OFF`                            | `OFF`        | If `ON`, build all unit tests and `lit` tests.                                                  |
| `MIM_LIT_TIMEOUT`       | `<timeout_in_sec>`                       | `20`         | Timeout for `lit` tests. <br> (requires `BUILD_TESTING=ON`).                                    |
| `MIM_LIT_WITH_VALGRIND` | `ON` \| `OFF`                            | `OFF`        | If `ON`, run the Mim CLI in the `lit` tests under Valgrind. <br> (requires `BUILD_TESTING=ON`). |

### Dependencies

In addition to the provided [submodules](https://github.com/AnyDSL/MimIR/tree/master/external), you will need:

- a recent version of [CMake](https://cmake.org/),
- a C++20-compatible C++ compiler, and
- optionally [LLVM](https://llvm.org/).

Mim emits LLVM IR, but it does _not_ link against LLVM.
So you can simply hand the emitted `*.ll` file to your system's LLVM toolchain.
Strictly speaking, LLVM is not required unless you want to continue from emitted LLVM IR.

## Coding Style

Use the following coding conventions:

- Class/type names use `CamelCase`.
- Constants defined in an `enum` or via `static const` use `Camel_Snake_Case`.
- Macro names use `SNAKE_IN_ALL_CAPS`.
- Everything else - variables, functions, etc. - uses `snake_case`.
- Use a trailing underscore for `private_or_protected_member_variables_`.
- Methods/functions that return a `bool` should be prefixed with `is_`.
- Methods/functions that return a `std::optional` or a pointer that may be `nullptr` should be prefixed with `isa_`.
- Do **not** do this for a `public_member_variable`.
- Use `struct` for [plain old data](https://en.cppreference.com/w/cpp/named_req/PODType).
- Use `class` for everything else.
- Prefer `// C++-style comments` over `/* C-style comments */`.
- Use `#pragma once` as the include guard for headers.
- Order visibility groups like this:
  1. `public`
  2. `protected`
  3. `private`

### Doxygen Style

- Use `///` for Doxygen comments.
- Use [Markdown-style](https://doxygen.nl/manual/markdown.html) Doxygen comments.
- Group related functions, methods, etc. via [named member groups](https://www.doxygen.nl/manual/grouping.html#memgroup).
- Capitalize the group name unless it is directly named after a method.

For all the smaller details such as indentation width, use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and the provided `.clang-format` file in the repository root.

To run `clang-format` automatically on changed files, install the provided [pre-commit](https://pre-commit.com/) hook:

```sh
pre-commit install
```

You can also [disable clang-format for a piece of code](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code) when necessary.
Depending on your editor, the [Vim integration](https://clang.llvm.org/docs/ClangFormat.html#vim-integration) and similar plugins may also be useful.

### Example

Here is a small header that follows the conventions above:

```cpp
#pragma once

namespace mim {

/// This is a cool class.
class Foo {
public:
    Foo(int foo, int bar)
        : foo_(foo)
        , bar_(bar) {}

    /// @name Getters
    ///@{
    int foo() const { return foo_; }
    int bar() const { return bar_; }
    ///@}

private:
    int foo_;
    int bar_;
};

} // namespace mim
```

## Debugging

**See also:**

- [Command-Line Reference](@ref clidebug)
- [GDB: A quick guide to make your debugging easier](https://johnnysswlab.com/gdb-a-quick-guide-to-make-your-debugging-easier/)
- [Advanced GDB Usage](https://interrupt.memfault.com/blog/advanced-gdb)
- [Debugging with GDB](https://sourceware.org/gdb/current/onlinedocs/gdb.html/)

### Dumping

You can directly invoke several MimIR dump helpers from within GDB, for example:

- [mim::Def::dump](@ref mim::Def::dump),
- [mim::Def::write](@ref mim::Def::write),
- [mim::World::dump](@ref mim::World::dump),
- [mim::World::write](@ref mim::World::write), ...

```gdb
(gdb) call def->dump()
(gdb) call def->dump(0)
(gdb) call def->dump(3)
(gdb) call world().write("out.mim")
```

In particular, note the different output levels of [mim::Def::dump](@ref mim::Def::dump).

You can also tweak the output behavior directly from within GDB by changing [mim::World::flags](@ref mim::World::flags) or [mim::World::log](@ref mim::World::log):

```gdb
(gdb) call world().flags().dump_gid = 1
(gdb) call world().flags().dump_recursive = 1
(gdb) call world().log().max_level_ = 4
```

Another useful trick is to recover a `Def*` from a [mim::Def::gid](@ref mim::Def::gid) via [mim::World::gid2def](@ref mim::World::gid2def):

```gdb
(gdb) p world().gid2def(123)
$1 = ...
(gdb) call $1->dump()
```

### Displaying DOT Graphs

`scripts/xdot.gdb` provides custom GDB commands to generate a [DOT](https://graphviz.org/) graph and display it with [xdot](https://github.com/jrfonseca/xdot.py).

To enable it, source `scripts/xdot.gdb` from your `~/.gdbinit`:

```gdb
source ~/mim/scripts/xdot.gdb
```

Here is the `xdot` GDB command in action:

![cgdb session using xdot](gdb-xdot.png)

\include "xdot-help.gdb"

### Conditional Breakpoints

Often, you will want to inspect a specific [mim::Def](@ref mim::Def) at a particular point in the program.
[Conditional breakpoints](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_33.html) are very handy for this.

For example, the following command breaks if the [mim::Def::gid](@ref mim::Def::gid) of variable `def` is `42` at source location `foo.cpp:23`:

```gdb
break foo.cpp:23 if def->gid() == 42
```

### Catching Exceptions

For several things, such as errors in the Mim frontend, MimIR uses C++ exceptions for error handling.
To stop when an exception is thrown, use:

```gdb
catch throw
```

### Valgrind & GDB

If you run into memory-related problems, it can be useful to run the program with [Valgrind's GDB server](https://valgrind.org/docs/manual/manual-core-adv.html).

Launch the program like this:

```sh
valgrind --vgdb=yes --vgdb-error=0 mim-gtest
```

and then follow the instructions printed by Valgrind.

## Tests {#tests}

### `lit` Tests

Run the [lit](https://llvm.org/docs/CommandGuide/lit.html) test suite with:

```sh
cmake --build build -t lit
```

You can also invoke the `lit` tests manually and filter for a specific test:

```sh
cd lit
./lit ../build/lit -a --filter foo.mim
```

If your build directory is actually called `build`, you can also use `probe.sh`:

```sh
cd lit
../scripts/probe.sh foo.mim
```

### Triggering Breakpoints from the Command Line

You can tell `mim` to trigger a breakpoint when certain events happen:

```sh
mim test.mim -b 1234            # Break if the node with gid 1234 is created.
mim test.mim -w 1234            # Break if the node with gid 1234 sets one of its operands.
mim test.mim --break-on-alpha   # Break if a check for alpha-equivalence fails.
```

See the [Command-Line Reference](@ref cli) for the full list of flags.

### GoogleTest

Run the [GoogleTest](https://google.github.io/googletest/) unit tests from the `build` directory with:

```sh
ctest
```

You can additionally enable [Valgrind](https://valgrind.org/) via:

```sh
ctest -T memcheck
```

During debugging, you will usually want to run only a specific test case.
You can [filter](https://github.com/google/googletest/blob/main/docs/advanced.md#running-a-subset-of-the-tests) tests like this:

```sh
./mim-gtest --gtest_filter="*Loc*"
```

This command lists all available tests:

```sh
./mim-gtest --gtest_list_tests
```

It can also be useful to turn assertion failures into debugger breakpoints:

```sh
./mim-gtest --gtest_break_on_failure
```

## Syntax Highlighting

[This](https://github.com/AnyDSL/vim-mim) Vim plugin provides syntax highlighting for Mim files.

There is also a [tree-sitter grammar](https://gitlab.com/amaeble/tree-sitter-mim) for Mim files and a [Helix fork](https://github.com/amaebel/helix) with highlight and injection queries.

## New Plugins

Check out the [demo](@ref demo) plugin for a minimal example.

You can create a new in-tree plugin `foobar` based on the [demo](@ref demo) plugin like this:

```sh
./scripts/new_plugin.sh foobar
```

### Third-Party Plugins

After installing MimIR, a third-party plugin only needs to find the `mim` package.
For example, to build a plugin called `foo`:

```cmake
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(foo)

find_package(mim)

add_mim_plugin(foo
    SOURCES
        mim/plug/foo/foo.h
        mim/plug/foo/foo.cpp
)
```

Configure the project with:

```cmake
cmake .. -Dmim_DIR=<MIM_INSTALL_PREFIX>/lib/cmake/mim
```

### `add_mim_plugin`

Registers a new MimIR plugin.

```cmake
add_mim_plugin(<plugin-name>
    [SOURCES <source>...]
    [PRIVATE <private-item>...]
    [INSTALL])
```

`<plugin-name>` is the name of the plugin.
Relative to the plugin's `CMakeLists.txt`, there should be a file `<plugin-name>.mim` containing the plugin's annexes.

The command creates two targets:

1. `mim_internal_<plugin-name>`

   This is an internal target used to bootstrap the plugin.
   It generates:
   - `<plugin-name>/autogen.h` for the C++ interface used to identify annexes,
   - `<plugin-name>.md` for the documentation, and
   - `<plugin-name>.d` for the plugin's dependencies.

   @note Tracking dependencies via the emitted dependency file is not supported by all CMake generators.
   See [`add_custom_command`'s `DEPFILE` argument](https://cmake.org/cmake/help/latest/command/add_custom_command.html).

2. `mim_<plugin-name>`

   This is the actual `MODULE` [library](https://cmake.org/cmake/help/latest/command/add_library.html).
   - `SOURCES`

     These are the `<source>` files used to build the loadable plugin containing normalizers, passes, and backends.
     One of the source files must export [`mim_get_plugin`](@ref mim::mim_get_plugin).

   - `PRIVATE`

     These are additional private build dependencies.

- `INSTALL`

  Specify this if the plugin description, plugin, and headers should be installed via `make install`.
  To export the targets, the export name `mim-targets` must be exported accordingly; see [`install(EXPORT ...)`](https://cmake.org/cmake/help/latest/command/install.html#export).

You can specify additional target properties in the plugin's `CMakeLists.txt`.
For example, the following snippet adds additional include paths to the `MODULE` target `mim_<plugin-name>`:

```cmake
target_include_directories(mim_<plugin-name> <path>...)
```
