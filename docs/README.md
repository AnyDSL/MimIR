# Introduction

[TOC]

<table>
    <tr>
        <td>Documentation</td>
        <td><a href=https://anydsl.github.io/thorin2><img src="https://img.shields.io/badge/docs-master-yellowgreen?logo=gitbook&logoColor=white)" alt="Documentation"></a></td>
    </tr>
    <tr>
        <td>License</td>
        <td><a href="https://github.com/AnyDSL/thorin2/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/thorin2)" alt="License"></a></td>
    </tr>
    <tr>
        <td>Requirements</td>
        <td>
            <a href="https://cmake.org/"><img src="https://img.shields.io/badge/cmake-3.7-blue.svg?logo=cmake)" alt="CMake"></a>
            <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/standard-C%2B%2B%2020-blue.svg?logo=C%2B%2B)" alt="C++"></a>
            <a href="https://github.com/AnyDSL/thorin2/tree/master/modules"><img src="https://img.shields.io/badge/submodules-5-blue?logo=git&logoColor=white)" alt="Submodules"></a>
        </td>
        <td>Tests</td>
        <td>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml"><img src="https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml/badge.svg?branch=master)" alt="Build & Test"></a>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml"><img src="https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml/badge.svg?branch=master)" alt="Doxygen"></a>
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
