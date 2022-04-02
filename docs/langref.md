# Language Reference

[TOC]

## Lexical Structure {#lex}

Thorin files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded.

### Definitions

We use the following rules specified via [regular expressions](https://en.wikipedia.org/wiki/Regular_expression) as shorthand:

| Name | Regular Expression                                      | Comment                                         |
|------|---------------------------------------------------------|-------------------------------------------------|
| 0b   | `0` \[ `b``B` \]                                        | prefix for binary literals                      |
| 0o   | `0` \[ `o``O` \]                                        | prefix for octal literals                       |
| 0x   | `0` \[ `x``X` \]                                        | prefix for hexadecimal literals                 |
| bin  | \[ `0``1` \]                                            | binary digit                                    |
| oct  | \[ `0`-`7` \]                                           | octal digit                                     |
| dec  | \[ `0`-`9` \]                                           | decimal digit                                   |
| sub  | \[ `₀`-`₉` \]                                           | subscript digit (always decimal)                |
| hex  | \[ `0`-`9``a`-`f``A`-`F` \]                             | hexadecimal digit                               |
| eE   | \[ `e` `E` \]                                           | exponent in floating point literals             |
| pP   | \[ `p` `P` \]                                           | exponent in floating point hexadecimal literals |
| sign | \[ `+` `-` \]                                           |                                                 |
| sym  | \[ `_``a`-`z``A`-`Z` \]\[ `_``0`-`9``a`-`z``A`-`Z` \]\* | symbol                                          |

### Terminals {#terminals}

The actual *[terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)* are specified in the following two tables:

| Primary Terminals               | Secondary Terminals                      | Comment              |
|---------------------------------|------------------------------------------|----------------------|
| `(` `)` `[` `]` `{` `}`         |                                          | delimiters           |
| `‹` `›` `«` `»`                 | `<<` `>>` `<` `>`                        | UTF-8 delimiters     |
| `→` `∷` `⊥` `⊤` `★` `□` `λ` `∀` | `->` `::` `.bot` `.top` `*` `.` `\` `\/` | further UTF-8 tokens |
| `=` `,` `;` `.` `#`             |                                          | further tokens       |

The [grammatical rules](#productions) will directly reference these *[terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)* via the *primary token*.
*Secondary terminals* are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the **same** lexical element.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.
Both tokens are identified as `⊥`.

| Terminal | Regular Expression                   | Comment                                                                                           |
|----------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| Id       | sym                                  | identifier                                                                                        |
| Ax       | `:` sym `.` sym ( `.` sym)?          | Axiom                                                                                             |
| L        | dec+                                 | unsigned decimal literal                                                                          |
| L        | 0b bin+                              | unsigned binary literal                                                                           |
| L        | 0o oct+                              | unsigned octal literal                                                                            |
| L        | 0x hex+                              | unsigned hexadecimal literal                                                                      |
| L        | sign dec+                            | signed decimal literal                                                                            |
| L        | sign 0b bin+                         | signed binary literal                                                                             |
| L        | sign 0o oct+                         | signed octal literal                                                                              |
| L        | sign 0x hex+                         | signed hexadecimal literal                                                                        |
| L        | sign? dec+ eE sign dec+              | floating-point literal                                                                            |
| L        | sign? dec+ `.` dec\* (eE sign dec+)? | floating-point literal                                                                            |
| L        | sign? dec\* `.` dec+ (eE sign dec+)? | floating-point literal                                                                            |
| L        | sign? 0x hex+ pP sign dec+           | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L        | sign? 0x hex+ `.` hex\* pP sign dec+ | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L        | sign? 0x hex\* `.` hex+ pP sign dec+ | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| I        | dec+ sub+                            | integer literal of type `:Int mod`                                                                |
| I        | dec+ `_` dec+                        | integer literal of type `:Int mod`                                                                |
| I        | `.ff` `.tt`                          | just an alias for `0₂` and `1₂`                                                                   |

Lexical elements follow the [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy to resolve ambiguities.
For Example, `<<<` is lexed as `<<` and `<`.

## Grammar

Thorin's grammar is defined as a [context-free grammar](https://en.wikipedia.org/wiki/Context-free_grammar) that consists of the *terminals* defined [above](#terminals) as well as the *nonterminals* and *productions* defined [below](#productions).
The start symbol is "m" (module).

### Productions {#productions}

The following table comprises all produciton rules:

| Nonterminal | Right-Hand Side                   | Comment                             | Thorin Class    |
|-------------|-----------------------------------|-------------------------------------|-----------------|
| m           | `.module` Id `{` e `}`            | module                              | thorin::World   |
| e           | L `∷` e                           | literal                             | thorin::Lit     |
| e           | e e                               | application                         | thorin::App     |
| e           | `λ` Id `:` e `.` e `→` e          | lambda                              | thorin::Lam     |
| e           | e `→` e                           | function type                       | thorin::Pi      |
| e           | `∀` Id `:` e `.` e `→` e          | dependent function type             | thorin::Pi      |
| e           | e `#` e                           | extract                             | thorin::Extract |
| e           | `.ins` `(` e `,` e `,` e `)`      | insert                              | thorin::Insert  |
| e           | `(` e `,` ... `,` e`)` ( `:` e )? | tuple with optional type ascription | thorin::Tuple   |
| e           | `[` e `,` ... `,` e `]`           | sigma                               | thorin::Sigma   |
| e           | Id `:` e `=` e `;` e              | let                                 | -               |

TODO

### Precedence

Expressions nesting is disambiguated according to the following precedence table:

| Operator | Description                  | Associativity |
|----------|------------------------------|---------------|
| L `∷` e  | type ascription of a literal | -             |
| e `#` e  | extract                      | left-to-right |
| e e      | application                  | left-to-right |
| e `→` e  | function type                | right-to-left |
