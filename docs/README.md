# Introduction

<a href="https://github.com/AnyDSL/thorin2"><img src="https://upload.wikimedia.org/wikipedia/commons/9/91/Octicons-mark-github.svg" alt="GitHub" height="28"/></a>
<a href="https://thorin2.readthedocs.io/en/latest/"><img src="https://read-the-docs-guidelines.readthedocs-hosted.com/_downloads/3ecfe564cae082d94611f6fda5e08d34/logo-wordmark-light.png" alt="Read the Docs" height="28"/></a>
<a href="https://github.com/AnyDSL/thorin2/actions/workflows/build-and-test.yml"><img src="https://github.com/AnyDSL/thorin2/workflows/build-and-test/badge.svg?branch=master" alt="build-and-test" height="28"/></a>
<a href="https://thorin2.readthedocs.io/en/latest/?badge=latest"><img src="https://readthedocs.org/projects/thorin2/badge/?version=latest" alt="Documentation Status" height="28"/></a>

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

* [The half library](https://sourceforge.net/projects/half/) library.

    This library lives in a subversion repository on Sourceforge.
    For this reason, we use a [git mirror](https://github.com/AnyDSL/half) to deploy it via a git submodule.

* [GoogleTest](https://github.com/google/googletest) for unit testing which is also deployed via a git submodule.
* Recent version of [CMake](https://cmake.org/)
* A C++20-compatible C++ compiler.
* While Thorin emits [LLVM](https://llvm.org/), it does *not* link to LLVM.

    Simply, toss the emitted `*.ll` file to your system's LLVM tool chain.
    But techincally, you don't need LLVM.
