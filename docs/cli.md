# Command-Line Reference {#cli}

[TOC]

## Usage

\include "cli-help.sh"

In addition, you can specify more search paths using the environment variable `MIM_PLUGIN_PATH`.
Mim will look for plugins in this priority:

1. The current working directory.
2. All paths specified via `-D` (in the given order).
3. All paths specified in the environment variable `MIM_PLUGIN_PATH` (in the given order).
4. `path/to/mim.exe/../../lib/mim`
5. `CMAKE_INSTALL_PREFIX/lib/mim`

## Debugging Features {#clidebug}

- You can increase the log level with `-V`.
  - No `-V` corresponds to mim::Log::Level::Error.
  - `-V` corresponds to mim::Log::Level::Warn.
  - `-VV` corresponds to mim::Log::Level::Info.
  - `-VVV` corresponds to mim::Log::Level::Verbose.
  - `-VVVV` corresponds to mim::Log::Level::Debug. This output only exists in a Debug build of MimIR.
- You can trigger a breakpoint upon construction of a mim::Def with a specific global id.

  For example, this will trigger a breakpoint if the mim::Def with [global id](@ref mim::Def::gid) `4223` is being created:

  ```
  mim -b 4223 in.mim
  ```

- You can also trigger a breakpoint at some other very specific places like when a check for alpha equivalence fails via `--break-on-alpha`.
