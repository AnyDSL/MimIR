# Introduction

[TOC]

<table  class="markdownTable">
    <tr class="markdownTableRowOdd">
        <td>Documentation</td>
        <td><a href=https://anydsl.github.io/thorin2><img src="https://img.shields.io/badge/docs-master-yellowgreen?logo=gitbook&logoColor=white" alt="Documentation"></a></td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            License
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/thorin2/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/thorin2?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAQCAQAAACxD+fXAAAACXBIWXMAAAAvAAAALwEOg5kqAAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAQlJREFUGBmFwbFK1HEAB/Bv3v1NuDvNwiuhREicurUXqA5bIu4Ip6bCBkFcHKygIRxCEbGlIKhd7AUOoTV6hXoH3+DTr+uEsMzPJ6e5527OY8d2zuOF5zmbMUve+uG7PV0X8jcd7/RN+OCTK5Z91MmfdO17pp7CipUUKqveu5MTFt3KiPuWMuK2xfym5arrJjNk2aMMaVkwo5Ff3FA3ZTZDnnicIde01cwm0TStMq3lUgprnqYwqamtbkoj5hMXXU7cTGHTagrzibbxxFxsGzjyxcB+Ci9tpLDjs4EjA29izAOvvNZXpbBnN4VxD23Z0lPPaQ4d5H9Uer75qqvKv6hZd+zEsTW1jPwEaIytGOF7PsEAAAAASUVORK5CYII=" alt="License"></a>
        </td>
    </tr>
    <tr class="markdownTableRowOdd">
        <td class="markdownTableBodyNone">
            Requirements
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://cmake.org/"><img src="https://img.shields.io/badge/cmake-3.7-blue.svg?logo=cmake" alt="CMake"></a>
            <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/standard-C%2B%2B%2020-blue.svg?logo=C%2B%2B" alt="C++"></a>
            <a href="https://llvm.org/"><img src="https://img.shields.io/badge/LLVM%2FClang-13-blue?logo=llvm" alt="LLVM/Clang"></a>
            <a href="https://github.com/AnyDSL/thorin2/tree/master/modules"><img src="https://img.shields.io/badge/submodules-5-blue?logo=git&logoColor=white" alt="Submodules"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            Tests
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/windows.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/windows?logo=windows&label=windows" alt="Windows"></a>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/linux.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/linux?logo=linux&label=linux&logoColor=white" alt="Linux"></a>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/doxygen?logo=github&label=doxygen" alt="Doxygen"></a>
        </td>
    </tr>
</table>

Thorin is an extensible compiler intermediate representation that is based upon the [Calculus of Constructions (CoC)](https://en.wikipedia.org/wiki/Calculus_of_constructions).
This means:
* [pure type system (PTS)](https://en.wikipedia.org/wiki/Pure_type_system)
* [higher-order functions](https://en.wikipedia.org/wiki/Higher-order_function)
* [dependent types](https://en.wikipedia.org/wiki/Dependent_type)

In contrast to other CoC-based program representations such as [Coq](https://coq.inria.fr/) or [Lean](https://leanprover.github.io/), Thorin is *not* a theorem prover but focuses on generating efficient code.
For this reason, Thorin explicitly features mutable state and models imperative control flow with [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style).

You can use Thorin either via it's C++-API or the [command-line utility](cli.md).

## Building

If you have a [GitHub account setup with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), just do this:
```
git clone --recurse-submodules git@github.com:AnyDSL/thorin2.git
```
Otherwise, clone via HTTPS:
```
git clone --recurse-submodules https://github.com/AnyDSL/thorin2.git
```
Then, build with:
```
cd thorin2
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j $(nproc)
```
For a `Release` build simply use `-DCMAKE_BUILD_TYPE=Release`.

### Tests

Run the tests within the `build` folder with:
```sh
ctest
```
In addition, you can enable [Valgrind](https://valgrind.org/) with:
```sh
ctest -T memcheck
```

### Documentation

You can build the documentation locally from the root folder with:
```sh
doxygen
```

## Dependencies

In addition to the provided submodules:

* Recent version of [CMake](https://cmake.org/)
* A C++20-compatible C++ compiler.
* While Thorin emits [LLVM](https://llvm.org/), it does *not* link against LLVM.

    Simply toss the emitted `*.ll` file to your system's LLVM toolchain.
    But techincally, you don't need LLVM.