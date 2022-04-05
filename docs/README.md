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
            <a href="https://github.com/AnyDSL/thorin2/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/thorin2?&color=yellowgreen&logo=data:image/png;base64,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LWIzYmEtYzNhMjA5ZDc5MWVkIgogICB4bXBNTTpJbnN0YW5jZUlEPSJ4bXAuaWlkOmZjZGUzNWI4LTg2MzAtNDgzZC1hYmEwLTc1NTExY2ZlNDU1NCIKICAgeG1wTU06T3JpZ2luYWxEb2N1bWVudElEPSJ4bXAuZGlkOjdhYzgxMGE2LTg2MGQtNGNkYy1iOWE5LWY3N2IwMzExMWFkYSIKICAgZGM6Rm9ybWF0PSJpbWFnZS9wbmciCiAgIEdJTVA6QVBJPSIyLjAiCiAgIEdJTVA6UGxhdGZvcm09IkxpbnV4IgogICBHSU1QOlRpbWVTdGFtcD0iMTY0OTIwMTMxMjE3Nzg5OCIKICAgR0lNUDpWZXJzaW9uPSIyLjEwLjMwIgogICB0aWZmOk9yaWVudGF0aW9uPSIxIgogICB4bXA6Q3JlYXRvclRvb2w9IkdJTVAgMi4xMCI+CiAgIDx4bXBNTTpIaXN0b3J5PgogICAgPHJkZjpTZXE+CiAgICAgPHJkZjpsaQogICAgICBzdEV2dDphY3Rpb249InNhdmVkIgogICAgICBzdEV2dDpjaGFuZ2VkPSIvIgogICAgICBzdEV2dDppbnN0YW5jZUlEPSJ4bXAuaWlkOjhiM2JlNzMyLTk2YmItNDU0ZC05OTVkLTJhMjExYTJmZWE5MiIKICAgICAgc3RFdnQ6c29mdHdhcmVBZ2VudD0iR2ltcCAyLjEwIChMaW51eCkiCiAgICAgIHN0RXZ0OndoZW49IjIwMjItMDQtMDZUMDE6Mjg6MzIrMDI6MDAiLz4KICAgIDwvcmRmOlNlcT4KICAgPC94bXBNTTpIaXN0b3J5PgogIDwvcmRmOkRlc2NyaXB0aW9uPgogPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4KICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgIAo8P3hwYWNrZXQgZW5kPSJ3Ij8+olNabAAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB+YEBRccIArnzK0AAAFUSURBVDjLjZJNK4RhFIavoxE2mo0sRCnbyUoWskVJWVmwmI2a/+DfqMlYy8JHVpKFpdkoUpZSpmShmbosHHp7h8ldp87HfZ/nuXseGAB1W90exKn8IZwDOsBW1mdANSLuy9woiJaBFWABuAVmgaUcXwGPQA24Ac4i4rJ8g0mgB2xERFetAk85242IjjoM7CW3z8Ib8BIR3azfgbVCTi5+SW6f70P1vFBX1eeMaqF/rh5+10PZnAC6wLW6mKd1gAvgInNydg10U/O1ANgBDoB9oD7g1erJOUjNz4IV4DQiHoApdewXi2PAVHJOU8OQugm8AjV1HmgDDXWkIB4BGkA7OTXgVd1EPbIfbbWitjIq2SvjCHVcbaofak89Uafz5Jbaynw6Z73kNtXxosdhdbTk+2dBoTeaH2ow1HW1k7HOf6HOqKvqXcHrXfZm/rPg2L9xXOZ/AnfuUG684dl9AAAAAElFTkSuQmCC" alt="License"></a>
        </td>
    </tr>
    <tr class="markdownTableRowOdd">
        <td class="markdownTableBodyNone">
            Requirements
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://cmake.org/"><img src="https://img.shields.io/badge/cmake-3.7-blue.svg?logo=cmake" alt="CMake"></a>
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
