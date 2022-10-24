set -e

../../build/bin/thorin -d affine brussel.thorin --output-thorin - -VVVV --output-ll brussel.ll
clang++ brussel_lib.cpp brussel.ll -o brussel.out -lc -Wno-override-module -O3
