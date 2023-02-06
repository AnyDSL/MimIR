ghc --make test.hs -o test.out
ghc -O3 --make test.hs -o test_O3.out
echo "Running test.out"
./test.out
echo "Running test_O3.out"
./test_O3.out
# rm -f test *.hi *.o
rm -f *.hi *.o
