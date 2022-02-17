# Language Reference

[TOC]

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
