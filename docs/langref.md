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
| sub   | \[ `₀-₉` \]                         | subscript digit   |
| sym   | \[ `_a-zA-Z` \]\[ `_0-9a-zA-Z` \]\* | symbol            |

### Lexical Elements

The actual lexical elements are specified in the following table:

| Name | Primary Token                                         | Alternative                              | Comment                                                                                           |
|------|-------------------------------------------------------|------------------------------------------|---------------------------------------------------------------------------------------------------|
|      | `(` `)` `[` `]` `{` `}`                               |                                          | delimiters                                                                                        |
|      | `‹` `›` `«` `»`                                       | `<<` `>>``<` `>`                         | UTF-8 delimiters                                                                                  |
|      | `→` `∷` `⊥` `⊤` `★` `□` `λ` `∀`                       | `->` `::` `.bot` `.top` `*` `.` `\` `\/` | further UTF-8 tokens                                                                              |
|      | `=` `,` `;` `.` `#`                                   |                                          | further tokens                                                                                    |
| Id   | sym                                                   |                                          | identifier                                                                                        |
| Ax   | `:` sym `.` sym ( `.` sym)?                           |                                          | Axiom                                                                                             |
| L    | dec+                                                  |                                          | unsigned decimal literal                                                                          |
| L    | `0b` bin+                                             |                                          | unsigned binary literal                                                                           |
| L    | `0o` oct+                                             |                                          | unsigned octal literal                                                                            |
| L    | `0x` hex+                                             |                                          | unsigned hexadecimal literal                                                                      |
| L    | \[ `+-` \]dec+                                        |                                          | signed decimal literal                                                                            |
| L    | \[ `+-` \] `0b` bin+                                  |                                          | signed binary literal                                                                             |
| L    | \[ `+-` \] `0o` oct+                                  |                                          | signed octal literal                                                                              |
| L    | \[ `+-` \] `0x` hex+                                  |                                          | signed hexadecimal literal                                                                        |
| L    | \[ `+-` \]? dec+ [ `eE` \]\[`+-`\] dec+               |                                          | floating-point literal                                                                            |
| L    | \[ `+-` \]? dec+ `.` dec\*([ `eE` \]\[`+-`\] dec+)?   |                                          | floating-point literal                                                                            |
| L    | \[ `+-` \]? dec\* `.` dec+([ `eE` \]\[`+-`\] dec+)?   |                                          | floating-point literal                                                                            |
| L    | \[ `+-` \]? `0x` hex+ [ `pP` \]\[`+-`\] dec+          |                                          | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L    | \[ `+-` \]? `0x` hex+ `.` hex\*[ `pP` \]\[`+-`\] dec+ |                                          | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L    | \[ `+-` \]? `0x` hex\* `.` hex+[ `pP` \]\[`+-`\] dec+ |                                          | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| I    | dec+sub+                                              | dec+ `_` dec +                           | integer literal of type `:Int mod`                                                                |
| I    | `.ff` `.tt`                                           |                                          | just an alias for `0₂` and `1₂`                                                                   |

The *name* is used to reference this element in the grammatical rules below.
If this field is empty, the grammatical rules will directly use the *primary token*.
*Alternatives* are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the *same* lexical element as its corresponding *primary token*.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.

Lexical elements follow the [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy to resolve ambiguities.
For Example, `<<<` is lexed as `<<` and `<`.

## Grammar

The following table comprises the full grammar:

| LHS | RHS                               | comment                             |
|-----|-----------------------------------|-------------------------------------|
| e   | L `∷` e                           | literal                             |
| e   | e e                               | application                         |
| e   | `λ` Id `:` e `.` e `→` e          | lambda                              |
| e   | e `→` e                           | function type                       |
| e   | `∀` Id `:` e `.` e `→` e          | dependent function type             |
| e   | e `#` e                           | extract                             |
| e   | `.ins` `(` e `,` e `,` e `)`      | insert                              |
| e   | `(` e `,` ... `,` e`)` ( `:` e )? | tuple with optional type ascription |
| e   | `[` e `,` ... `,` e `]`           | sigma                               |
| e   | Id `:` e `=` e `;` e              | let                                 |

TODO

<!--```ebnf-->
<!--(* nominals *)-->
<!--n = lam ID ":" e ["=" e "," e] ";"-->
  <!--| sig ID ":" e (["=" e "," ... "," e]) | ("(" N ")") ";"-->

<!--```-->

### Precedence

Expressions nesting is disambiguated according to the following precedence table:

| Operator    | Description                  | Associativity |
|-------------|------------------------------|---------------|
| literal`∷`e | type ascription of a literal | -             |
| e`#`e       | extract                      | left-to-right |
| e e         | application                  | left-to-right |
| e`→`e       | function type                | right-to-left |
