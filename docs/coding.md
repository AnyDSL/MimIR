# Coding & Debugging

[TOC]

This document comprises some information that is related to coding but does not directly deals with the API.

## Coding Style

Use the following coding convetions:
* class/type names in `CamelCase`
* constants as defined in an `enum` or via `static const` in `Camel_Snake_Case`
* macro names in `SNAKE_IN_ALL_CAPS`
* eveything else like variables, functions, etc. in `snake_case`
* use a traling underscore suffix for a `private_member_variable_`
* don't do that for a `public_member_variable`
* use `struct` for [plain old data](https://en.cppreference.com/w/cpp/named_req/PODType)
* use `class` for everything else
* visibility groups in this order:
    1. `public`
    2. `protected`
    3. `private`
* prefer `// C++-style comments` over `/* C-style comments */`
* use three slashes for Doxygen and [group](https://www.doxygen.nl/manual/grouping.html) your methods into logical units if possible

For all the other minute details like indentation width etc. use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and the provided `.clang-format` file in the root of the repository.
In order to run `clang-format` automatically on all changed files, switch to the provied pre-commit hook:
```sh
git config --local core.hooksPath .githooks/
```

# Debugging

## Logging

## Breakpoints
