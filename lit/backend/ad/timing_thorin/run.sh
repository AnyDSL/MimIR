clang pow.ll test.c -o test.out -Wno-override-module
clang -O3 pow.ll test.c -o test_O3.out -Wno-override-module

echo "Running test.out"
./test.out
echo "Running test_O3.out"
./test_O3.out
