# Command-Line Reference {#cli}

[TOC]

## Usage

\include "cli-help.md"

## Emitters {#emitters}

* `-e thorin` emits Thorin code again.
* `-e h` emits a header file to be used to interface with a dialect in C++.
* `-e md` emits the input formatted as [Markdown](https://www.doxygen.nl/manual/markdown.html).
    * Single-line `/// xxx` comments will put `xxx` directly into the Markdown output.
        You can put an optional space after the `///` that will be elided in the Markdown output.
    * Everything else will be put verbatim within a [fenced code block](https://www.doxygen.nl/manual/markdown.html#md_fenced).
* `-e dot` emit the Thorin program as a graph using [Graphviz' DOT language](https://graphviz.org/doc/info/lang.html).
* `-e ll` compiles the Thorin code to [LLVM](https://llvm.org/docs/LangRef.html).

## Debugging Features {#clidebug}

* You can increase the log level with `-V`.
    * No `-V` corresponds to thorin::LogLevel::Error.
    * `-V` corresponds to thorin::LogLevel::Warn.
    * `-VV` corresponds to thorin::LogLevel::Info.
    * `-VVV` corresponds to thorin::LogLevel::Verbose.
    * `-VVVV` corresponds to thorin::LogLevel::Debug. This output only exists in a Debug build of Thorin.

* You can trigger a breakpoint upon construction of a thorin::Def with a specific global id.

    For example, this will trigger a breakpoint if the thorin::Def with [global id](@ref thorin::Def::gid) `666` is being created:
    ```
    thorin -b 666 in.thorin
    ```
