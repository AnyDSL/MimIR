BASENAME=`basename $1 .hs`
OUT=${BASENAME}_opt
LLVM=${BASENAME}.ll
ghc -fllvm -keep-llvm-files -fforce-recomp -O2 $1 -o $OUT
mv $LLVM ${BASENAME}_opt.ll
