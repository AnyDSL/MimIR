BASENAME=`basename $1 .hs`
OUT=${BASENAME}_opt3
ghc -funfolding-use-threshold=16 -O2 -optc-O3 $1 -o $OUT
