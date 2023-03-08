# Command-Line Reference {#cli}

[TOC]

## Usage

\include "cli-help.inc"

## Debugging Features {#clidebug}

* You can increase the log level with `-V`.
    * No `-V` corresponds to thorin::Log::Level::Error.
    * `-V` corresponds to thorin::Log::Level::Warn.
    * `-VV` corresponds to thorin::Log::Level::Info.
    * `-VVV` corresponds to thorin::Log::Level::Verbose.
    * `-VVVV` corresponds to thorin::Log::Level::Debug. This output only exists in a Debug build of Thorin.

* You can trigger a breakpoint upon construction of a thorin::Def with a specific global id.

    For example, this will trigger a breakpoint if the thorin::Def with [global id](@ref thorin::Def::gid) `4223` is being created:
    ```
    thorin -b 4223 in.thorin
    ```
