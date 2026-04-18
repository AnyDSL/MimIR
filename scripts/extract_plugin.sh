#!/bin/bash
set -eu

print_help() {
    cat <<'EOF'
Usage: ./scripts/extract_plugin.sh <plugin_name>

Extract an existing in-tree plugin into extra/<plugin_name>.

The script:
  - moves src/mim/plug/<plugin_name>/ into extra/<plugin_name>/
  - moves include/mim/plug/<plugin_name>/ into extra/<plugin_name>/include/...
  - moves lit/<plugin_name>/ into extra/<plugin_name>/lit/
  - removes <plugin_name> from src/mim/plug/CMakeLists.txt
  - rewrites the extracted CMakeLists.txt for standalone find_package(mim) use

Arguments:
  <plugin_name>  Name of an existing in-tree plugin.

Options:
  -h, --help     Show this help text and exit.
EOF
}

rewrite_sources_for_out_of_tree() {
    local cmake_file=$1
    local tmp_file
    tmp_file=$(mktemp)

    awk '
        function trim(s) {
            gsub(/^[ \t]+|[ \t]+$/, "", s)
            return s
        }

        {
            stripped = trim($0)

            if (in_sources && (stripped == ")" || stripped ~ /^[A-Z_][A-Z0-9_]*$/)) {
                in_sources = 0
            }

            if (stripped == "SOURCES") {
                in_sources = 1
                print
                next
            }

            if (in_sources &&
                stripped ~ /^[^[:space:]#][^[:space:]]*\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$/ &&
                stripped !~ /^src\//) {
                match($0, /^[ \t]*/)
                indent = substr($0, RSTART, RLENGTH)
                print indent "src/" stripped
                next
            }

            print
        }
    ' "$cmake_file" > "$tmp_file"

    mv "$tmp_file" "$cmake_file"
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

if [[ $# -ne 1 ]]; then
    print_help >&2
    exit 1
fi

plugin=$1

if [[ ! "$plugin" =~ ^[A-Za-z0-9_]+$ ]]; then
    echo "Plugin names may only contain letters, digits, and underscores" >&2
    print_help >&2
    exit 1
fi

src_plugin_root="$root/src/mim/plug/$plugin"
include_plugin_root="$root/include/mim/plug/$plugin"
lit_plugin_root="$root/lit/$plugin"
extra_plugin_root="$root/extra/$plugin"
plug_cmake="$root/src/mim/plug/CMakeLists.txt"

if [[ ! -d "$src_plugin_root" ]]; then
    echo "In-tree plugin source directory does not exist: $src_plugin_root" >&2
    exit 1
fi

if [[ ! -f "$src_plugin_root/CMakeLists.txt" ]]; then
    echo "In-tree plugin CMakeLists.txt does not exist: $src_plugin_root/CMakeLists.txt" >&2
    exit 1
fi

if [[ ! -f "$src_plugin_root/$plugin.mim" ]]; then
    echo "In-tree plugin description does not exist: $src_plugin_root/$plugin.mim" >&2
    exit 1
fi

if [[ -e "$extra_plugin_root" ]]; then
    echo "Target directory already exists: $extra_plugin_root" >&2
    exit 1
fi

if ! grep -Eq "^[[:space:]]+$plugin$" "$plug_cmake"; then
    echo "Plugin '$plugin' is not listed in $plug_cmake" >&2
    exit 1
fi

mkdir -p "$extra_plugin_root/src"
mkdir -p "$extra_plugin_root/include/mim/plug"

mv "$src_plugin_root/CMakeLists.txt" "$extra_plugin_root/CMakeLists.txt"
mv "$src_plugin_root/$plugin.mim" "$extra_plugin_root/$plugin.mim"

shopt -s dotglob nullglob
for path in "$src_plugin_root"/*; do
    mv "$path" "$extra_plugin_root/src/"
done
shopt -u dotglob nullglob
rmdir "$src_plugin_root"

if [[ -d "$include_plugin_root" ]]; then
    mv "$include_plugin_root" "$extra_plugin_root/include/mim/plug/"
fi

if [[ -d "$lit_plugin_root" ]]; then
    mkdir -p "$extra_plugin_root/lit"
    shopt -s dotglob nullglob
    for path in "$lit_plugin_root"/*; do
        mv "$path" "$extra_plugin_root/lit/"
    done
    shopt -u dotglob nullglob
    rmdir "$lit_plugin_root"
fi

tmp_file=$(mktemp)
cat > "$tmp_file" <<EOF
cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(${plugin})

if(NOT COMMAND add_mim_plugin)
    find_package(mim REQUIRED)
endif()

EOF
cat "$extra_plugin_root/CMakeLists.txt" >> "$tmp_file"
mv "$tmp_file" "$extra_plugin_root/CMakeLists.txt"

rewrite_sources_for_out_of_tree "$extra_plugin_root/CMakeLists.txt"

sed -i "/^[[:space:]]*${plugin}$/d" "$plug_cmake"

if git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$root" add -f "$extra_plugin_root"
    git -C "$root" add "$plug_cmake"
    git -C "$root" add -u "$root/src/mim/plug" "$root/include/mim/plug" "$root/lit"
fi

echo "Extracted in-tree plugin '$plugin' to '$extra_plugin_root'"
