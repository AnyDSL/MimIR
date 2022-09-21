# Introduction

[TOC]

<table  class="markdownTable">
    <tr class="markdownTableRowOdd">
        <td>Support</td>
        <td>
            <a href=https://anydsl.github.io/thorin2><img src="https://img.shields.io/badge/docs-master-green?logo=gitbook&logoColor=white" alt="Documentation"></a>
            <a href=https://discord.gg/FPp7hdj3fQ><img src="https://img.shields.io/discord/960975142459179068?color=green&logo=discord&logoColor=white" alt="Discord"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            License
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/thorin2/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/thorin2?&color=yellowgreen&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH5gQFFyMP+ejbyAAAAVRJREFUOMuNkk0rhGEUhq+jETaajSxEKdvJShayRUlZWbCYjZr/4N+oyVjLwkdWkoWl2ShSllKmZKGZuiwcenuHyV2nzsd9n+e5ex4YAHVb3R7EqfwhnAM6wFbWZ0A1Iu7L3CiIloEVYAG4BWaBpRxfAY9ADbgBziLisnyDSaAHbEREV60CTznbjYiOOgzsJbfPwhvwEhHdrN+BtUJOLn5Jbp/vQ/W8UFfV54xqoX+uHn7XQ9mcALrAtbqYp3WAC+Aic3J2DXRT87UA2AEOgH2gPuDV6sk5SM3PghXgNCIegCl17BeLY8BUck5Tw5C6CbwCNXUeaAMNdaQgHgEaQDs5NeBV3UQ9sh9ttaK2MirZK+MIdVxtqh9qTz1Rp/PkltrKfDpnveQ21fGix2F1tOT7Z0GhN5ofajDUdbWTsc5/oc6oq+pdwetd9mb+s+DYv3Fc5n8Cd+5Qbrzh2X0AAAAASUVORK5CYII=" alt="License"></a>
        </td>
    </tr>
    <tr class="markdownTableRowOdd">
        <td class="markdownTableBodyNone">
            Requirements
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://cmake.org/"><img src="https://img.shields.io/badge/cmake-3.20-blue.svg?logo=cmake" alt="CMake"></a>
            <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/standard-C%2B%2B%2020-blue.svg?logo=C%2B%2B" alt="C++"></a>
            <a href="https://llvm.org/"><img src="https://img.shields.io/badge/LLVM%2FClang-13.x--14.x-blue?logo=llvm" alt="LLVM/Clang"></a>
            <a href="https://github.com/AnyDSL/thorin2/tree/master/modules"><img src="https://img.shields.io/badge/submodules-5-blue?logo=git&logoColor=white" alt="Submodules"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            Tests
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/linux.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/linux?logo=linux&label=linux&logoColor=white" alt="Linux"></a>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/windows.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/windows?logo=windows&label=windows" alt="Windows"></a>
            <a href="https://github.com/AnyDSL/thorin2/actions/workflows/macos.yml"><img src="https://img.shields.io/github/workflow/status/anydsl/thorin2/macos?logo=apple&label=macos" alt="MacOS"></a>
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

### Install

If you want to install Thorin, specify an install prefix and build the target `install`:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/my/local/install/prefix
cmake --build build -j $(nproc) -t install
```

### Build Switches

| CMake Switch           | Options                                  | Default      | Comment                                                                             |
|------------------------|------------------------------------------|--------------|-------------------------------------------------------------------------------------|
| `CMAKE_BUILD_TYPE`     | `Debug` \| `Release` \| `RelWithDebInfo` | `Debug`      | Build type.                                                                         |
| `CMAKE_INSTALL_PREFIX` |                                          | `/usr/local` | Install prefix.                                                                     |
| `THORIN_BUILD_DOCS`    | `ON` \| `OFF`                            | `OFF`        | If `ON`, Thorin will build the documentation <br> (requires Doxygen).               |
| `THORIN_BUILD_TESTING` | `ON` \| `OFF`                            | `OFF`        | If `ON`, Thorin will build all of Thorin's own tests.                               |
| `THORIN_ENABLE_CHECKS` | `ON` \| `OFF`                            | `ON`         | If `ON`, enables expensive runtime checks <br> (requires `CMAKE_BUILD_TYPE=Debug`). |

## Dependencies

In addition to the provided submodules:

* Recent version of [CMake](https://cmake.org/)
* A C++20-compatible C++ compiler.
* While Thorin emits [LLVM](https://llvm.org/), it does *not* link against LLVM.

    Simply toss the emitted `*.ll` file to your system's LLVM toolchain.
    But techincally, you don't need LLVM.
