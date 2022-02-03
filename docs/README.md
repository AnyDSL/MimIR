# Introduction

[![build-and-test](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml/badge.svg?branch=master)](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml)
[![doxygen](https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml/badge.svg?branch=master)](https://github.com/AnyDSL/thorin2/actions/workflows/doxygen.yml)
<a href="https://anydsl.github.io/thorin2/"><img src="https://anydsl.github.io/thorin2/doxygen.svg" alt="Doxygen" height="20"/></a>

Thorin is an extensible compiler intermediate representation that is based upon the [Calculus of Constructions (CoC)](https://en.wikipedia.org/wiki/Calculus_of_constructions). This means:
* [pute type system (PTS)](https://en.wikipedia.org/wiki/Pure_type_system)
* [higher-order functions](https://en.wikipedia.org/wiki/Higher-order_function)
* [dependent types](https://en.wikipedia.org/wiki/Dependent_type)

In contrast to other CoC-based program representations such as [Coq](https://coq.inria.fr/) or [Lean](https://leanprover.github.io/), Thorin is *not* a theorem prover but focuses on generating efficient code. For this reason, Thorin  explicitly features mutable state and models imperative control flow via [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style).

## Building

If you have a [GitHub account setup with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), just do this:
```bash
git clone --recurse-submodules git@github.com:AnyDSL/thorin2.git
```
Otherwise, clone via HTTPS:
```sh
git clone --recurse-submodules https://github.com/AnyDSL/thorin2.git
```
Then, build with:
```sh
cd thorin2
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release # or Debug
cmake --build build -j $(nproc)
```
For a debug build simply use `-DCMAKE_BUILD_TYPE=Debug`.

### Dependencies

* [Half library](https://sourceforge.net/projects/half/).

    This library originally resides in a subversion repository on Sourceforge.
    For this reason, we use a [git mirror](https://github.com/AnyDSL/half) to deploy it via a git submodule.

* [GoogleTest](https://github.com/google/googletest) for unit testing which is also deployed via a git submodule.
* Recent version of [CMake](https://cmake.org/)
* A C++20-compatible C++ compiler.
* While Thorin emits [LLVM](https://llvm.org/), it does *not* link against LLVM.

    Simply toss the emitted `*.ll` file to your system's LLVM toolchain.
    But techincally, you don't need LLVM.
