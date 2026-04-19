# Plugins {#plugins}

[TOC]

Check out the [demo](@ref demo) plugin for a minimal example.
It uses our custom [`add_mim_plugin`](@ref add_mim_plugin_cmake) CMake command.

Plugin names may only contain letters, digits, and underscores, and are limited to 8 characters.

## Create a New In-Tree Plugin

Create a new in-tree plugin `foobar` based on the [demo](@ref demo) plugin:

```sh
./scripts/new_plugin.sh foobar
```

The script also supports `-h`/`--help` and prints the same usage text when called incorrectly.

By default, the script creates an in-tree plugin and updates `src/mim/plug/CMakeLists.txt`.
The generated files are:

* `src/mim/plug/<plugin>/<plugin>.mim`
* `src/mim/plug/<plugin>/CMakeLists.txt`
* `src/mim/plug/<plugin>/<plugin>.cpp`
* `src/mim/plug/<plugin>/normalizers.cpp`
* `include/mim/plug/<plugin>/<plugin>.h`
* `lit/<plugin>/const.mim`

## Create a Standalone Third-Party Plugin

To create a self-contained third-party plugin repository in `extra/`, use:

```sh
./scripts/new_plugin.sh foobar --extra
```

This creates `extra/<plugin>/` with:

* `<plugin>.mim`
* `CMakeLists.txt`
* `src/<plugin>.cpp`
* `src/normalizers.cpp`
* `include/mim/plug/<plugin>/<plugin>.h`
* `lit/const.mim`

In `--extra` mode, the script also initializes a new Git repository for the plugin.

## Extract an Existing In-Tree Plugin

To move an existing in-tree plugin into `extra/foobar`, use:

```sh
./scripts/extract_plugin.sh foobar
```

This moves:

* `src/mim/plug/<plugin>/` into `extra/<plugin>/`
* `include/mim/plug/<plugin>/` into `extra/<plugin>/include/...`
* `lit/<plugin>/` into `extra/<plugin>/lit/`

It also rewrites the extracted `CMakeLists.txt` for out-of-tree use and removes the plugin from the in-tree plugin list so it is picked up through `extra/` instead.

## Third-Party Plugin Discovery

If you clone a plugin repository into `extra/`, MimIR picks it up automatically during configuration when the repository contains a `CMakeLists.txt` as a direct child of `extra/`.

If the plugin repository also contains `lit/*.mim` tests, they are picked up automatically by the main `lit` target as well.

## Standalone Third-Party Builds

After installing MimIR, a third-party plugin only needs to find the `mim` package.
For example, a plugin called `foo` can be set up like this:

```cmake
cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(foo)

if(NOT COMMAND add_mim_plugin)
    find_package(mim REQUIRED)
endif()

add_mim_plugin(foo
    SOURCES
        src/foo.cpp
        src/normalizers.cpp
)
```

Configure the project standalone with:

```sh
cmake .. -Dmim_DIR=<MIM_INSTALL_PREFIX>/lib/cmake/mim
```

The authoritative reference for `add_mim_plugin` itself lives in [`cmake/Mim.cmake`](@ref add_mim_plugin_cmake).
