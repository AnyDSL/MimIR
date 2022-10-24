for f in *.hs ; do
    ./compile3.sh $f
    ./compile2.sh $f
    ./compile.sh $f
done

./cleanup.sh
