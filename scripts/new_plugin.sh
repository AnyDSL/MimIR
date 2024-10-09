#!/bin/bash
set -eu

# Create a new plugin called <plugin_name> derived from the demo plugin:
# ./new_plugin.sh plugin_name

root=$(dirname $0)/..
plugin=$1

mkdir -p $root/include/mim/plug/$plugin
mkdir -p $root/src/mim/plug/$plugin

for demo_file in $root/src/mim/plug/demo/demo.mim     \
                 $root/src/mim/plug/demo/CMakeLists.txt  \
                 $root/src/mim/plug/demo/demo.cpp        \
                 $root/src/mim/plug/demo/normalizers.cpp \
                 $root/include/mim/plug/demo/demo.h
do
    plugin_file="${demo_file//demo/$plugin}"
    cp $demo_file $plugin_file
    sed -i -e "s/demo/${plugin}/g" ${plugin_file}
    git add $plugin_file
done
