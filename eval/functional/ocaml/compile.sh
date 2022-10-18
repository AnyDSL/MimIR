BASENAME=`basename $1 .ml`
ocamlc $1 -o $BASENAME
ocamlopt -S $1 -o $BASENAME.opt
# ocamlopt -O3
