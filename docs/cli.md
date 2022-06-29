# Command-Line Reference {#cli}

[TOC]

## Usage

\include "cli-help.md"

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
