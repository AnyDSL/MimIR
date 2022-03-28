# Introduction

[TOC]

[![documentation](https://img.shields.io/badge/documentation-master-blue)](https://anydsl.github.io/thorin2)
[![GitHub](https://img.shields.io/github/license/anydsl/thorin2)](https://github.com/AnyDSL/thorin2/blob/master/LICENSE.TXT)
[![build-and-test](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml/badge.svg?branch=master)](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml)
[![doxygen](https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml/badge.svg?branch=master)](https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml)

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

* Recent version of [CMake](https://cmake.org/)
* A C++20-compatible C++ compiler.
* While Thorin emits [LLVM](https://llvm.org/), it does *not* link against LLVM.

    Simply toss the emitted `*.ll` file to your system's LLVM toolchain.
    But techincally, you don't need LLVM.

### Deployed as a Git Submodule

The following dependencies are deployed via a [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules):

* [Half library](https://sourceforge.net/projects/half/)

    This *header-only* library provides a `half` type that mimics the builtin types `float` or `double`.
    It originally resides in a subversion repository on Sourceforge.
    For this reason, we use a [git mirror](https://github.com/AnyDSL/half) to deploy it as a git submodule.

* [Lyra](https://www.bfgroup.xyz/Lyra/)

    *Header-only* library for parsing command line arguments.

* [GoogleTest](https://github.com/google/googletest)

    This is optional for running the unit tests.

* [Doxygen](https://www.doxygen.nl/index.html) and [Doxygen Awesome](https://jothepro.github.io/doxygen-awesome-css/)

    This is optional for building the documentation.
