# Coding & Debugging

[TOC]

This document comprises some information that is related to coding but does not directly deals with the API.

## Coding Style

Use the following coding conventions:
* class/type names in `CamelCase`
* constants as defined in an `enum` or via `static const` in `Camel_Snake_Case`
* macro names in `SNAKE_IN_ALL_CAPS`
* everything else like variables, functions, etc. in `snake_case`
* use a trailing underscore suffix for a `private_member_variable_`
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
As a utility to make debugging Thorin itself less painful with certain debuggers, the `thorin.natvis` file can be loaded for getting more expressive value inspection.
In VS Code you can do so by adding the following to the `launch.json` configurations. When launching from VS Code via CMake, put it in `settings.json`'s `"cmake.debugConfig":`:
```json
"visualizerFile": "${workspaceFolder}/thorin.natvis",
"showDisplayString": true,
```

## Logging

## Breakpoints
