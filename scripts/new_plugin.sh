#!/bin/bash
set -eu

# Create a new plugin called <plugin_name> derived from the demo plugin:
#   ./new_plugin.sh plugin_name
#   ./new_plugin.sh plugin_name --extra

root=$(cd "$(dirname "$0")/.." && pwd)
plugin=$1
mode="${2:-}"

demo_src="$root/src/mim/plug/demo"
demo_inc="$root/include/mim/plug/demo"

if [[ "$mode" == "--extra" ]]; then
    plugin_root="$root/extra/$plugin"
    mkdir -p "$plugin_root"

    # Create out-of-tree structure
    mkdir -p "$plugin_root/src"
    mkdir -p "$plugin_root/include/mim/plug/$plugin"

    # Copy files
    cp "$demo_src/demo.mim"             "$plugin_root/$plugin.mim"
    cp "$demo_src/CMakeLists.txt"       "$plugin_root/CMakeLists.txt"
    cp "$demo_src/demo.cpp"             "$plugin_root/src/$plugin.cpp"
    cp "$demo_src/normalizers.cpp"      "$plugin_root/src/normalizers.cpp"
    cp "$demo_inc/demo.h"               "$plugin_root/include/mim/plug/$plugin/$plugin.h"

    # Replace demo -> plugin
    find "$plugin_root" -type f -print0 | xargs -0 sed -i -e "s/demo/${plugin}/g"

    # Initialize new git repo
    (
        cd "$plugin_root"
        git init
        git add .
        git commit -m "Initial commit: ${plugin} plugin"
    )

    echo "Created out-of-tree plugin repo in: $plugin_root"
    exit 0
fi

# in-tree mode (old behavior)
mkdir -p "$root/include/mim/plug/$plugin"
mkdir -p "$root/src/mim/plug/$plugin"

for demo_file in "$root/src/mim/plug/demo/demo.mim" \
    "$root/src/mim/plug/demo/CMakeLists.txt" \
    "$root/src/mim/plug/demo/demo.cpp" \
    "$root/src/mim/plug/demo/normalizers.cpp" \
    "$root/include/mim/plug/demo/demo.h"; do
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
