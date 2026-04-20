#!/bin/bash
set -eu

print_help() {
    cat <<'EOF'
Usage: ./scripts/new_plugin.sh <plugin_name> [--extra]

Create a new plugin derived from the demo plugin.

Arguments:
  <plugin_name>  Plugin name using only letters, digits, and underscores.
                 The name must not exceed 8 characters.

Options:
  --extra        Create a standalone third-party plugin repository in extra/.
  -h, --help     Show this help text and exit.
EOF
}

root=$(cd "$(dirname "$0")/.." && pwd)

if [[ $# -eq 0 ]]; then
    print_help >&2
    exit 1
fi

case "${1:-}" in
    -h|--help)
        print_help
        exit 0
        ;;
esac

if [[ $# -lt 1 || $# -gt 2 ]]; then
    print_help >&2
    exit 1
fi

plugin=$1
mode="${2:-}"

if [[ -n "$mode" && "$mode" != "--extra" ]]; then
    echo "Unknown option: $mode" >&2
    print_help >&2
    exit 1
fi

if [[ ! "$plugin" =~ ^[A-Za-z0-9_]+$ ]]; then
    echo "Plugin names may only contain letters, digits, and underscores" >&2
    print_help >&2
    exit 1
fi

if [[ ${#plugin} -gt 8 ]]; then
    echo "Plugin names may not exceed 8 characters" >&2
    print_help >&2
    exit 1
fi

demo_src="$root/src/mim/plug/demo"
demo_inc="$root/include/mim/plug/demo"
demo_lit="$root/lit/demo"

if [[ "$mode" == "--extra" ]]; then
    plugin_root="$root/extra/$plugin"
    mkdir -p "$plugin_root"

    # Create out-of-tree structure
    mkdir -p "$plugin_root/src"
    mkdir -p "$plugin_root/include/mim/plug/$plugin"
    mkdir -p "$plugin_root/lit"

    # Copy files
    cp "$demo_src/demo.mim"             "$plugin_root/$plugin.mim"
    cp "$demo_src/CMakeLists.txt"       "$plugin_root/CMakeLists.txt"
    cp "$demo_src/demo.cpp"             "$plugin_root/src/$plugin.cpp"
    cp "$demo_src/normalizers.cpp"      "$plugin_root/src/normalizers.cpp"
    cp "$demo_inc/demo.h"               "$plugin_root/include/mim/plug/$plugin/$plugin.h"
    cp "$demo_lit/const.mim"            "$plugin_root/lit/const.mim"

    # Replace demo -> plugin
    find "$plugin_root" -type f -print0 | xargs -0 sed -i -e "s/demo/${plugin}/g"
    sed -i \
        -e "1i cmake_minimum_required(VERSION 3.25 FATAL_ERROR)\nproject(${plugin})\n\nif(NOT COMMAND add_mim_plugin)\n    find_package(mim REQUIRED)\nendif()\n" \
        -e "s#^[[:space:]]*${plugin}\\.cpp#        src/${plugin}.cpp#" \
        -e "s#^[[:space:]]*normalizers\\.cpp#        src/normalizers.cpp#" \
        "$plugin_root/CMakeLists.txt"

    # Initialize new git repo
    (
        cd "$plugin_root"
        git init
        git add .
        git commit -m "Initial commit: ${plugin} plugin"
    )

    echo "Created out-of-tree plugin repo in: $plugin_root"
    exit 0
else
    # in-tree mode
    mkdir -p "$root/include/mim/plug/$plugin"
    mkdir -p "$root/src/mim/plug/$plugin"
    mkdir -p "$root/lit/$plugin"

    for demo_file in "$root/src/mim/plug/demo/demo.mim" \
        "$root/src/mim/plug/demo/CMakeLists.txt" \
        "$root/src/mim/plug/demo/demo.cpp" \
        "$root/src/mim/plug/demo/normalizers.cpp" \
        "$root/include/mim/plug/demo/demo.h" \
        "$root/lit/demo/const.mim"; do
        plugin_file="${demo_file//demo/$plugin}"
        cp "$demo_file" "$plugin_file"
        sed -i -e "s/demo/${plugin}/g" "$plugin_file"
        git add "$plugin_file"
    done

    # Update the main CMakeLists.txt with the new plugin in alphabetical order
    cmake_file="$root/src/mim/plug/CMakeLists.txt"

    # Extract current plugins, add new one, sort, and replace the list
    plugins=$(awk '/^set\(MIM_PLUGINS/,/^\)/' "$cmake_file" | tail -n +2 | head -n -1 | tr -d ' ' | sort -u)
    updated_plugins=$(echo -e "$plugins\n$plugin" | sort -u)

    # Reconstruct the plugin list
    new_plugin_block="set(MIM_PLUGINS\n"
    while read -r line; do
        new_plugin_block+="    $line\n"
    done <<<"$updated_plugins"
    new_plugin_block+=")"

    # Replace the block in the file
    perl -0777 -i -pe "s/set\(MIM_PLUGINS.*?\)/$new_plugin_block/s" "$cmake_file"
    git add "$cmake_file"
fi
