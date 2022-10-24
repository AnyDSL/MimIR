ghc -fllvm -keep-llvm-files -fforce-recomp $1 -o `basename $1 .hs`
