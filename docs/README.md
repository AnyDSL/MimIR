# Introduction

[TOC]

<table  class="markdownTable">
    <tr class="markdownTableRowOdd">
        <td>Support</td>
        <td>
            <a href=https://anydsl.github.io/MimIR><img src="https://img.shields.io/badge/docs-master-green?logo=gitbook&logoColor=white" alt="Documentation"></a>
            <a href=https://discord.gg/FPp7hdj3fQ><img src="https://img.shields.io/discord/960975142459179068?color=green&logo=discord&logoColor=white" alt="Discord"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            License
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/MimIR/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/MimIR?&color=yellowgreen&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH5gQFFyMP+ejbyAAAAVRJREFUOMuNkk0rhGEUhq+jETaajSxEKdvJShayRUlZWbCYjZr/4N+oyVjLwkdWkoWl2ShSllKmZKGZuiwcenuHyV2nzsd9n+e5ex4YAHVb3R7EqfwhnAM6wFbWZ0A1Iu7L3CiIloEVYAG4BWaBpRxfAY9ADbgBziLisnyDSaAHbEREV60CTznbjYiOOgzsJbfPwhvwEhHdrN+BtUJOLn5Jbp/vQ/W8UFfV54xqoX+uHn7XQ9mcALrAtbqYp3WAC+Aic3J2DXRT87UA2AEOgH2gPuDV6sk5SM3PghXgNCIegCl17BeLY8BUck5Tw5C6CbwCNXUeaAMNdaQgHgEaQDs5NeBV3UQ9sh9ttaK2MirZK+MIdVxtqh9qTz1Rp/PkltrKfDpnveQ21fGix2F1tOT7Z0GhN5ofajDUdbWTsc5/oc6oq+pdwetd9mb+s+DYv3Fc5n8Cd+5Qbrzh2X0AAAAASUVORK5CYII=" alt="License"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            Tests
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/linux.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/linux.yml?logo=linux&label=linux&logoColor=white&branch=master" alt="Linux"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/windows.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/windows.yml?logo=windows&label=windows&branch=master" alt="Windows"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/macos.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/macos.yml?logo=apple&label=macos&branch=master" alt="MacOS"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/doxygen.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/doxygen.yml?logo=github&label=doxygen&branch=master" alt="Doxygen"></a>
        </td>
    </tr>
</table>

**MimIR** is an extensible compiler intermediate representation that is based upon the [Calculus of Constructions (CoC)](https://en.wikipedia.org/wiki/Calculus_of_constructions).
This means:

- [pure type system (PTS)](https://en.wikipedia.org/wiki/Pure_type_system)
- [higher-order functions](https://en.wikipedia.org/wiki/Higher-order_function)
- [dependent types](https://en.wikipedia.org/wiki/Dependent_type)

In contrast to other CoC-based program representations such as [Coq](https://coq.inria.fr/) or [Lean](https://leanprover.github.io/), MimIR is _not_ a theorem prover but focuses on generating efficient code.
For this reason, MimIR explicitly features mutable state and models imperative control flow with [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style).

You can use MimIR either via it's C++-API or through its frontend language [Mim](langref.md).

## Building

If you have a [GitHub account setup with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), just do this:

```
git clone --recurse-submodules git@github.com:AnyDSL/MimIR.git
```

Otherwise, clone via HTTPS:

```
git clone --recurse-submodules https://github.com/AnyDSL/MimIR.git
```

Then, build with:

```
cd mim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j $(nproc)
```

For a `Release` build simply use `-DCMAKE_BUILD_TYPE=Release`.

### Install

If you want to install MimIR, specify an install prefix and build the target `install`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/my/local/install/prefix
cmake --build build -j $(nproc) -t install
```

### Build Switches

| CMake Switch            | Options                                  | Default      | Comment                                                                                               |
| ----------------------- | ---------------------------------------- | ------------ | ----------------------------------------------------------------------------------------------------- |
| `CMAKE_BUILD_TYPE`      | `Debug` \| `Release` \| `RelWithDebInfo` | `Debug`      | Build type.                                                                                           |
| `CMAKE_INSTALL_PREFIX`  |                                          | `/usr/local` | Install prefix.                                                                                       |
| `MIM_BUILD_DOCS`        | `ON` \| `OFF`                            | `OFF`        | If `ON`, build the documentation <br> (requires Doxygen).                                             |
| `MIM_BUILD_EXAMPLES`    | `ON` \| `OFF`                            | `OFF`        | If `ON`, build the examples.                                                                          |
| `MIM_ENABLE_CHECKS`     | `ON` \| `OFF`                            | `ON`         | If `ON`, enables expensive runtime checks <br> (requires `CMAKE_BUILD_TYPE=Debug`).                   |
| `BUILD_TESTING`         | `ON` \| `OFF`                            | `OFF`        | If `ON`, build all unit and lit tests.                                                                |
| `MIM_LIT_TIMEOUT`       | `<timeout_in_sec>`                       | `20`         | Timeout for lit tests. <br> (requires `BUILD_TESTING=ON`).                                            |
| `MIM_LIT_WITH_VALGRIND` | `ON` \| `OFF`                            | `OFF`        | If `ON`, the Mim CLI in the lit tests will be run under valgrind. <br> (requires `BUILD_TESTING=ON`). |

## Dependencies

In addition to the provided [submodules](https://github.com/AnyDSL/MimIR/tree/master/external):

- Recent version of [CMake](https://cmake.org/)
- A C++20-compatible C++ compiler.
- While Mim emits [LLVM](https://llvm.org/), it does _not_ link against LLVM.

  Simply toss the emitted `*.ll` file to your system's LLVM toolchain.
  But technically, you don't need LLVM.

## MimIR vs Mim

MimIR is a recursive acronym for _MimIR is my intermediate Representation_.
[Mímir or Mim](https://en.wikipedia.org/wiki/M%C3%ADmir) is also a figure in Norse mythology, renowned for his knowledge and wisdom, who is beheaded during the &AElig;sir–Vanir War.
Afterward, the god Odin carries around Mímir's head and it recites secret knowledge and counsel to him.
Now, you have Mímir's head at your fingertips.

We use the term **MimIR**, if we speak of the internal graph-based representation and its C++-API, and the term **Mim**, if we mean MimIR's frontend language.
To keep things short and simple, all code consistently uses `mim` or `MIM` for namespaces, macro prefixes, etc.
And if in doubt, just use **Mim**.
