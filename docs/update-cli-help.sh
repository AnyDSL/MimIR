#! /bin/bash

THORIN=$1
if [[ $1 == "" ]]; then
    THORIN=build/bin/thorin
fi
$THORIN -h 2>&1 | head -n -1 - > `dirname $0`/cli-help.md
