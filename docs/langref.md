# Language Reference {#langref}

[TOC]

## Lexical Structure {#lex}

Thorin files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded and [lexed](https://en.wikipedia.org/wiki/Lexical_analysis) from left to right.
The [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy resolves any ambiguities in the lexical rules below.
For Example, `<<<` is lexed as `<<` and `<`.

### Terminals {#terminals}

The actual *[terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)* are specified in the following tables.

#### Primary Terminals

The [grammatical rules](#grammar) will directly reference these *primary [terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)*.
*Secondary terminals* are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the **same** lexical element as its corresponding *primary token*.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.
Both tokens are identified as `⊥`.

| Primary Terminals           | Secondary Terminals                         | Comment                   |
|-----------------------------|---------------------------------------------|---------------------------|
| `(` `)` `[` `]` `{` `}`     |                                             | delimiters                |
| `‹` `›` `«` `»`             | `<<` `>>` `<` `>`                           | UTF-8 delimiters          |
| `→` `⊥` `⊤` `★` `□` `λ` `Π` | `->` `.bot` `.top` `*` `.lm` <tt>\|~\|</tt> | further UTF-8 tokens      |
| `=` `,` `;` `.` `#` `:` `%` |                                             | further tokens            |
| `<eof>`                     |                                             | marks the end of the file |

In addition you can use `⟨`, `⟩`, `⟪`, and `⟫` as an alternative for `‹`, `›`, `«`, and `»`.

#### Keywords

In addition the following keywords are *terminals*:

| Terminal    | Comment                                           |
|-------------|---------------------------------------------------|
| `.ax`       | axiom                                             |
| `.let`      | let expression                                    |
| `.Pi`       | nominal thorin::Pi                                |
| `.con`      | [continuation](@ref thorin::Lam) (declaration)    |
| `.fun`      | [function](@ref thorin::Lam) (declaration - TODO) |
| `.lam`      | [lambda](@ref thorin::Lam) (declaration)          |
| `.cn`       | [continuation](@ref thorin::Lam) (expression)     |
| `.fn`       | [function](@ref thorin::Lam) (expression - TODO)  |
| `.cn`       | [lambda](@ref thorin::Lam) (expression)           |
| `.Arr`      | nominal thorin::Arr                               |
| `.pack`     | nominal thorin::Pack                              |
| `.Sigma`    | nominal thorin::Sigma                             |
| `.def`      | nominal definition                                |
| `.extern`   | marks nominal as external                         |
| `.ins`      | thorin::Insert expression                         |
| `.insert`   | alias for `.ins`                                  |
| `.module`   | starts a module                                   |
| `.import`   | imports a dialect                                 |
| `.Nat`      | thorin::Nat                                       |
| `.Idx`      | thorin::Idx                                       |
| `.Bool`     | alias for `.Idx 2`                                |
| `.ff`       | alias for `0₂`                                    |
| `.tt`       | alias for `1₂`                                    |
| `.Type`     | thorin::Type                                      |
| `.Univ`     | thorin::Univ                                      |

All keywords start with a `.` to prevent name clashes with identifiers.

#### Regular Expressions

The following *terminals* comprise more complicated patterns that are specified via [regular expressions](https://en.wikipedia.org/wiki/Regular_expression):

| Terminal | Regular Expression                   | Comment                                                                                           |
|----------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| Sym      | sym                                  | symbol                                                                                            |
| Ax       | `%` sym `.` sym ( `.` sym)?          | Axiom                                                                                             |
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
| I        | dec+ sub+                            | index literal of type `.Idx sub`                                                                  |
| I        | dec+ `_` dec+                        | index literal of type `.Idx sub`                                                                  |

The previous table resorts to the following definitions as shorthand:

| Name | Regular Expression                                         | Comment                                         |
|------|------------------------------------------------------------|-------------------------------------------------|
| 0b   | `0` \[ `b``B` \]                                           | prefix for binary literals                      |
| 0o   | `0` \[ `o``O` \]                                           | prefix for octal literals                       |
| 0x   | `0` \[ `x``X` \]                                           | prefix for hexadecimal literals                 |
| bin  | \[ `0``1` \]                                               | binary digit                                    |
| oct  | \[ `0`-`7` \]                                              | octal digit                                     |
| dec  | \[ `0`-`9` \]                                              | decimal digit                                   |
| sub  | \[ `₀`-`₉` \]                                              | subscript digit (always decimal)                |
| hex  | \[ `0`-`9``a`-`f``A`-`F` \]                                | hexadecimal digit                               |
| eE   | \[ `e` `E` \]                                              | exponent in floating point literals             |
| pP   | \[ `p` `P` \]                                              | exponent in floating point hexadecimal literals |
| sign | \[ `+` `-` \]                                              |                                                 |
| sym  | \[ `_``a`-`z``A`-`Z` \]\[ `.``_``0`-`9``a`-`z``A`-`Z` \]\* | symbol                                          |

So, *sym* refers to the shorthand rule while *Sym* refers to the *terminal* that is identical to *sym*.
However, the terminal *Ax* also uses the shorthand rule *sym*.

### Comments

In addition, the following comments are available:
* `/* ... */`: multi-line comment
* `//`: single-line comment
* `///`: single-line comment for [Markdown](https://www.doxygen.nl/manual/markdown.html) [output](@ref cli):
    - Single-line `/// xxx` comments will put `xxx` directly into the Markdown output.
        You can put an optional space after the `///` that will be elided in the Markdown output.
    - Everything else will be put verbatim within a [fenced code block](https://www.doxygen.nl/manual/markdown.html#md_fenced).

## Grammar {#grammar}

Thorin's grammar is defined as a [context-free grammar](https://en.wikipedia.org/wiki/Context-free_grammar) that consists of the *terminals* defined [above](#terminals) as well as the *nonterminals* and *productions* defined below.
The start symbol is "m" (module).

The following tables comprise all production rules:

### Module

| Nonterminal | Right-Hand Side   | Comment | Thorin Class  |
|-------------|-------------------|---------|---------------|
| m           | i\* d\*           | module  | thorin::World |
| i           | `.import` Sym `;` | import  |               |

### Declarations

| Nonterminal | Right-Hand Side                                                   | Comment                            | Thorin Class  |
|-------------|-------------------------------------------------------------------|------------------------------------|---------------|
| d           | `.ax` Ax `:` e<sub>type</sub> `;`                                 | axiom                              | thorin::Axiom |
| d           | `.let` p  `=` e `;`                                               | let                                | -             |
| d           | `.Pi` Sym ( `:` e<sub>type</sub> )? `,` e<sub>dom</sub> n         | Pi declaration                     | thorin::Pi    |
| d           | `.con` Sym p                       n                              | continuation declaration           | thorin::Lam   |
| d           | `.fun` Sym p `→` e<sub>codom</sub> n                              | function declaration               | thorin::Lam   |
| d           | `.lam` Sym p `→` e<sub>codom</sub> n                              | lambda declaration                 | thorin::Lam   |
| d           | `.Arr` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n   | array declaration                  | thorin::Arr   |
| d           | `.pack` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n  | pack declaration                   | thorin::Pack  |
| d           | `.Sigma` Sym ( `:` e<sub>type</sub> )? `,` L<sub>arity</sub> v? n | sigma declaration                  | thorin::Sigma |
| d           | `.def` Sym n                                                      | nominal definition                 | nominals      |
| n           | `;` \| o                                                          | nominal definition                 | -             |
| o           | `=` de `;`                                                        | operand of definition              | -             |
| o           | `=` `{` e `,` ... `,` e  `}` `;`                                  | operands of definition<sup>s</sup> | -             |
<sup>s</sup> opens new scope

### Patterns

| Nonterminal | Right-Hand Side               | Comment                  |
|-------------|-------------------------------|--------------------------|
| p           | Sym t                         | identifier pattern       |
| p           | s `(` p `,` ... `,` p `)` t   | tuple pattern            |
| p           | s `[` b `,` ... `,` b `]` t   | sigma pattern            |
| b           | s e<sub>type</sub>            | identifier binder        |
| b           | s `[` b `,` ... `,` b `]` t   | sigma binder             |
| t           | ( `:` e<sub>type</sub> )?     | optional type ascription |
| s           | ( Sym `::` )?                 | optional symbol          |

Note that you **can** switch from a pattern to a binder (from `p` to `b` inside a pattern), but not vice versa.
For this reason there is no rule `b -> s (p, ..., p)`.

### Expressions

| Nonterminal | Right-Hand Side                                                               | Comment                             | Thorin Class    |
|-------------|-------------------------------------------------------------------------------|-------------------------------------|-----------------|
| de          | d\* e                                                                         | declaration expression              | -               |
| e           | `.Univ`                                                                       | universise: type of a type level    | thorin::Univ    |
| e           | `.Type` e                                                                     | type of level e                     | thorin::Type    |
| e           | `*`                                                                           | alias for `.Type (0:.Univ)`         | thorin::Type    |
| e           | `□`                                                                           | alias for `.Type (1:.Univ)`         | thorin::Type    |
| e           | `.Nat`                                                                        | natural number                      | thorin::Nat     |
| e           | `.Idx`                                                                        | builtin of type `.Nat -> *`         | thorin::Idx     |
| e           | `.Bool`                                                                       | alias for `.Idx 2`                  | thorin::Idx     |
| e           | `{` de `}`                                                                    | block<sup>s</sup>                   | -               |
| e           | L `:` e<sub>type</sub>                                                        | literal                             | thorin::Lit     |
| e           | `.ff`                                                                         | alias for `0_2`                     | thorin::Lit     |
| e           | `.tt`                                                                         | alias for `1_2`                     | thorin::Lit     |
| e           | ( `.bot` \| `.top` ) ( `:` e<sub>type</sub> )?                                | bottom/top                          | thorin::TExt    |
| e           | Sym                                                                           | identifier                          | -               |
| e           | Ax                                                                            | use of an axiom                     | -               |
| e           | e e                                                                           | application                         | thorin::App     |
| e           | Sym `:` e<sub>dom</sub> `→` e<sub>codom</sub> `.` e<sub>body</sub>            | lambda<sup>s</sup>                  | thorin::Lam     |
| d           | `.cn` Sym p                       `=` de                                      | continuation expression             | thorin::Lam     |
| d           | `.fn` Sym p `→` e<sub>codom</sub> `=` de                                      | function expression                 | thorin::Lam     |
| d           | `λ`   Sym p `→` e<sub>codom</sub> `=` de                                      | lambda expression                   | thorin::Lam     |
| e           | e<sub>dom</sub> `→` e<sub>codom</sub>                                         | function type                       | thorin::Pi      |
| e           | `Π` b `→` e<sub>codom</sub>                                                   | dependent function type<sup>s</sup> | thorin::Pi      |
| e           | e `#` Sym                                                                     | extract via field "Sym"             | thorin::Extract |
| e           | e `#` e<sub>index</sub>                                                       | extract                             | thorin::Extract |
| e           | `.ins` `(` e<sub>tuple</sub> `,` e<sub>index</sub> `,` e<sub>value</sub> ` )` | insert                              | thorin::Insert  |
| e           | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>` )` ( `:` e<sub>type</sub> )?   | tuple                               | thorin::Tuple   |
| e           | `[` b `,` ... `,` b `]`                                                       | sigma<sup>s</sup>                   | thorin::Sigma   |
| e           | `‹` i e<sub>shape</sub> `;` e<sub>body</sub>`›`                               | pack<sup>s</sup>                    | thorin::Pack    |
| e           | `«` i e<sub>shape</sub> `;` e<sub>body</sub>`»`                               | array<sup>s</sup>                   | thorin::Arr     |
<sup>s</sup> opens new scope

An elided type of
* a literal defaults to `.Nat`,
* a bottom/top defaults to `*`,
* a nominals defaults to `*`.

#### Precedence

Expressions nesting is disambiguated according to the following precedence table (from strongest to weakest binding):

| Operator             | Description                         | Associativity |
|----------------------|-------------------------------------|---------------|
| L `:` e              | type ascription of a literal        | -             |
| e `#` e              | extract                             | left-to-right |
| e e                  | application                         | left-to-right |
| `Π` Sym `:` e        | domain of a dependent function type | -             |
| `.fun` Sym Sym `:` e | nominal funciton declaration        | -             |
| `.lam` Sym Sym `:` e | nominal continuation declaration    | -             |
| `.fn` Sym `:` e      | nominal funciton expression         | -             |
| `.lm` Sym `:` e      | nominal continuation expression     | -             |
| e `→` e              | function type                       | right-to-left |

Note that the domain of a dependent function type binds slightly stronger than `→`.
This has the effect that, e.g., `Π T: * → T → T` has the expected binding like this: (`Π T: *`) `→` (`T → T`).
Otherwise, `→` would be consumed by the domain: `Π T:` (`* →` (`T → T`)) ↯.
A similar situation occurs for a `.lam` declaration.

## Scoping

Thorin uses [_lexical scoping_](https://en.wikipedia.org/wiki/Scope_(computer_science)#Lexical_scope) where all names live within the same namespace - with a few exceptions noted below.
The grammar tables above also indiciate which constructs open new scopes (and close them again).

### Underscore

The symbol `_` is special and never binds to an entity.
For this reason, `_` can be bound over and over again within the same scope (without effect).
Hence, using the symbol `_` will always result in a scoping error.

### Pis

Note that _only_ `Π x: e → e` introduces a new scope.
`x: e → e` is a syntax error.
If the variable name of a Pi's domain is elided and the domain is a sigma, its elements will be imported into the Pi's scope to make these elements available in the Pi's codomain:
```
Π [T: *, U: *] → [T, U]
```

### Axioms

The names of axioms are special and live in a global namespace.

### Field Names of Sigmas

Named elements of nominal sigmas are avaiable for extracts/inserts.
These names take precedence over the usual scope.
In the following example, `i` refers to the first element `i` of `X` and **not** to the `i` introduced via `.let`:
```
.let i = 1_2;
Π X: [i: .Nat, j: .Nat] → f X#i;
```
Use parentheses to refer to the `.let`-bounded `i`:
```
.let i = 1_2;
Π X: [i: .Nat, j: .Nat] → f X#(i);
```
