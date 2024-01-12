#!/bin/bash
set -eu

root=$(dirname $0)/..
plugin=$1

mkdir -p $root/include/thorin/plug/$plugin
mkdir -p $root/src/thorin/plug/$plugin

for demo_file in $root/src/thorin/plug/demo/demo.thorin         \
                 $root/src/thorin/plug/demo/CMakeLists.txt      \
                 $root/src/thorin/plug/demo//demo.cpp           \
                 $root/src/thorin/plug/demo//normalizers.cpp    \
                 $root/include/thorin/plug/demo/demo.h
do
    plugin_file="${demo_file//demo/$plugin}"
    cp $demo_file $plugin_file
    sed -i -e "s/demo/${plugin}/g" ${plugin_file}
    git add $plugin_file
done
