# Introduction

[![build-and-test](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml/badge.svg?branch=master)](https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml)
<a href="https://anydsl.github.io/thorin2/"><img src="https://anydsl.github.io/thorin2/doxygen.svg" alt="Doxygen" height="20"/></a>

Thorin is a compiler intermediate representation.

## Building

```bash
git clone --recurse-submodules git@github.com:AnyDSL/thorin2.git
cd thorin2
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
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
