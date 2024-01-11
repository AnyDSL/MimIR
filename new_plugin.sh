#!/bin/bash
set -eu

plugin=$1

mkdir -p $plugin/include/thorin/plug/$plugin
mkdir -p $plugin/src/thorin/plug/$plugin

for demo_file in demo/demo.thorin demo/CMakeLists.txt demo/include/thorin/plug/demo/demo.h demo/src/thorin/plug/demo/demo.cpp demo/src/thorin/plug/demo/normalizers.cpp ; do
    plugin_file="${demo_file//demo/$plugin}"
    cp $demo_file $plugin_file
    sed -i -e "s/demo/${plugin}/g" ${plugin_file}
    git add $plugin_file
done
