# Language Reference

[TOC]

## Lexical Structure

Thorin files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded.

### Building Blocks

We use the following rules as shorthand:

| Class | Definition                          | Comment           |
|-------|-------------------------------------|-------------------|
| bin   | \[ `01` \]                          | binary digit      |
| oct   | \[ `0-7` \]                         | octal digit       |
| dec   | \[ `0-9` \]                         | decimal digit     |
| hex   | \[ `0-9a-fA-F` \]                   | hexadecimal digit |
| sym   | \[ `_a-zA-Z` \]\[ `_0-9a-zA-Z` \]\* | symbol            |

### Lexical Elements

The actual lexical elements are specified in the following table:

| Name | Primary Token                                         | Alternative           | Comment                                                                                           |
|------|-------------------------------------------------------|-----------------------|---------------------------------------------------------------------------------------------------|
|      | `(` `)` `[` `]` `{` `}`                               |                       | delimiters                                                                                        |
|      | `‹` `›` `«` `»`                                       | `<<` `>>``<` `>`      | UTF-8 delimiters and ASCII-variants                                                               |
|      | `=` `,` `;` `.`                                       |                       | punctuators                                                                                       |
|      | `∷`                                                   | `::`                  | UTF-8 punctuators and ASCII-variants                                                              |
|      | `⊥` `⊤` `★` `□`                                       | `.bot` `.top` `*` `.` | constants                                                                                         |
| Id   | sym                                                   |                       | identifier                                                                                        |
| Ax   | `:` sym `.` sym ( `.` sym)?                           |                       | Axiom                                                                                             |
| Lu   | dec+                                                  |                       | unsigned decimal literal                                                                          |
| Lu   | `0b` bin+                                             |                       | unsigned binary literal                                                                           |
| Lu   | `0o` oct+                                             |                       | unsigned octal literal                                                                            |
| Lu   | `0x` hex+                                             |                       | unsigned hexadecimal literal                                                                      |
| Ls   | \[ `+-` \]dec+                                        |                       | signed decimal literal                                                                            |
| Ls   | \[ `+-` \] `0b` bin+                                  |                       | signed binary literal                                                                             |
| Ls   | \[ `+-` \] `0o` oct+                                  |                       | signed octal literal                                                                              |
| Ls   | \[ `+-` \] `0x` hex+                                  |                       | signed hexadecimal literal                                                                        |
| Lr   | \[ `+-` \]? dec+ [ `eE` \]\[`+-`\] dec+               |                       | floating-point literal                                                                            |
| Lr   | \[ `+-` \]? dec+ `.` dec\*([ `eE` \]\[`+-`\] dec+)?   |                       | floating-point literal                                                                            |
| Lr   | \[ `+-` \]? dec\* `.` dec+([ `eE` \]\[`+-`\] dec+)?   |                       | floating-point literal                                                                            |
| Lr   | \[ `+-` \]? `0x` hex+ [ `pP` \]\[`+-`\] dec+          |                       | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| Lr   | \[ `+-` \]? `0x` hex+ `.` hex\*[ `pP` \]\[`+-`\] dec+ |                       | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| Lr   | \[ `+-` \]? `0x` hex\* `.` hex+[ `pP` \]\[`+-`\] dec+ |                       | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |

The *name* is used to reference this element in the grammatical rules below.
If this field is empty, the grammatical rules will directly use the *primary token*.
*Alternatives* are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the *same* lexical element as its corresponding *primary token*.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.

Lexical elements follow the [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy to resolve ambiguities.
For Example, `<<<` is lexed as `<<` and `<`.

## Grammar

```ebnf
(* nominals *)
n = lam ID ":" e ["=" e "," e] ";"
  | sig ID ":" e (["=" e "," ... "," e]) | ("(" N ")") ";"

e = e e                             (* application *)
  | e#e                             (* extract *)
  | .ins(e, e, e)                   (* insert *)
  | "[" e "," ... "," e "]"         (* sigma *)
  | "(" e "," ... "," e")" [":" e]  (* tuple *)
  | ID: e = e; e                    (* let *)
```
