# Command-Line Reference {#cli}

[TOC]

## Usage

\include "cli-help.sh"

In addition, you can specify more search paths using the environment variable `THORIN_PLUGIN_PATH`.
Thorin will look for plugins in this priority:
1. The current working directory.
2. All paths specified via `-D` (in the given order).
3. All paths specified in the environment variable `THORIN_PLUGIN_PATH` (in the given order).
4. `path/to/thorin.exe/../../lib/thorin`
5. `CMAKE_INSTALL_PREFIX/lib/thorin`

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
    thorin -b 4223 in.mim
    ```
* You can also trigger a breakpoint at some other very specific places like when a check for alpha equivalence fails via `--break-on-alpha-unequal`.
