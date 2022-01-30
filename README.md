# Thorin 2

The Higher-Order Intermediate Representation

## Build Instructions

```
git clone --recurse-submodules git@github.com:AnyDSL/thorin2.git
cd thorin2
mkdir build
cd build
cmake ..
make
```

## Documentation

See (Read the Docs)[https://readthedocs.org/projects/thorin2/].

## Syntax

```ebnf
(* nominals *)
n = lam ID ":" e ["=" e "," e] ";"
  | sig ID ":" e (["=" e "," ... "," e]) | ("(" N ")") ";"

e = e e                             (* application *)
  | ex(e, e)                        (* extract *)
  | ins(e, e, e)                    (* insert *)
  | "[" e "," ... "," e "]"         (* sigma *)
  | "(" e "," ... "," e")" [":" e]  (* tuple *)
  | ID: e = e; e                    (* let *)
```
