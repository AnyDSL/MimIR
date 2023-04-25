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

| Terminal  | Comment                                                   |
|-----------|-----------------------------------------------------------|
| `.ax`     | axiom                                                     |
| `.let`    | let expression                                            |
| `.Pi`     | mutable thorin::Pi                                        |
| `.con`    | [continuation](@ref thorin::Lam) (declaration)            |
| `.fun`    | [function](@ref thorin::Lam) (declaration - TODO)         |
| `.lam`    | [lambda](@ref thorin::Lam) (declaration)                  |
| `.cn`     | [continuation](@ref thorin::Lam) (expression)             |
| `.fn`     | [function](@ref thorin::Lam) (expression - TODO)          |
| `.cn`     | [lambda](@ref thorin::Lam) (expression)                   |
| `.Arr`    | mutable thorin::Arr                                       |
| `.pack`   | mutable thorin::Pack                                      |
| `.Sigma`  | mutable thorin::Sigma                                     |
| `.def`    | mutable definition                                        |
| `.extern` | marks mutable as external                                 |
| `.ins`    | thorin::Insert expression                                 |
| `.insert` | alias for `.ins`                                          |
| `.module` | starts a module                                           |
| `.import` | imports another Thorin file                               |
| `.plugin` | like `.import` and additionally loads the compiler plugin |
| `.Nat`    | thorin::Nat                                               |
| `.Idx`    | thorin::Idx                                               |
| `.Bool`   | alias for `.Idx 2`                                        |
| `.ff`     | alias for `0₂`                                            |
| `.tt`     | alias for `1₂`                                            |
| `.Type`   | thorin::Type                                              |
| `.Univ`   | thorin::Univ                                              |

All keywords start with a `.` to prevent name clashes with identifiers.

#### Regular Expressions

The following *terminals* comprise more complicated patterns that are specified via [regular expressions](https://en.wikipedia.org/wiki/Regular_expression):

| Terminal      | Regular Expression                   | Comment                                                                                           |
|---------------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| Sym           | sym                                  | symbol                                                                                            |
| Ax            | `%` sym `.` sym ( `.` sym)?          | Axiom                                                                                             |
| L             | dec+                                 | unsigned decimal literal                                                                          |
| L             | 0b bin+                              | unsigned binary literal                                                                           |
| L             | 0o oct+                              | unsigned octal literal                                                                            |
| L             | 0x hex+                              | unsigned hexadecimal literal                                                                      |
| L             | sign dec+                            | signed decimal literal                                                                            |
| L             | sign 0b bin+                         | signed binary literal                                                                             |
| L             | sign 0o oct+                         | signed octal literal                                                                              |
| L             | sign 0x hex+                         | signed hexadecimal literal                                                                        |
| L             | sign? dec+ eE sign dec+              | floating-point literal                                                                            |
| L             | sign? dec+ `.` dec\* (eE sign dec+)? | floating-point literal                                                                            |
| L             | sign? dec\* `.` dec+ (eE sign dec+)? | floating-point literal                                                                            |
| L             | sign? 0x hex+ pP sign dec+           | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L             | sign? 0x hex+ `.` hex\* pP sign dec+ | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| L             | sign? 0x hex\* `.` hex+ pP sign dec+ | [floating-point hexadecimal](https://en.cppreference.com/w/cpp/language/floating_literal) literal |
| I<sub>n</sub> | dec+ sub+<sub>n</sub>                | index literal of type `.Idx n`                                                                    |
| I<sub>n</sub> | dec+ `_` dec+<sub>n</sub>            | index literal of type `.Idx n`                                                                    |

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

| Notation  | Meaning                                           |
|-----------|---------------------------------------------------|
| a, ..., a | comma separated list of zero or more "a" elements |
| a\*       | zero or more repetitions of "a"                   |
| a\+       | one or more repetitions of "a"                    |
| (a)?      | "a" is optional                                   |


The following tables comprise all production rules:

### Module

| Nonterminal | Right-Hand Side   | Comment | Thorin Class                |
|-------------|-------------------|---------|-----------------------------|
| m           | l\* d\*           | module  | [World](@ref thorin::World) |
| l           | `.import` Sym `;` | import  |                             |
| l           | `.plugin` Sym `;` | plugin  |                             |

### Declarations

| Nonterminal | Right-Hand Side                                                   | Comment                            | Thorin Class                |
|-------------|-------------------------------------------------------------------|------------------------------------|-----------------------------|
| d           | `.ax` Ax `:` e<sub>type</sub> `;`                                 | axiom                              | [Axiom](@ref thorin::Axiom) |
| d           | `.let` p  `=` e `;`                                               | let                                | -                           |
| d           | `.Pi` Sym ( `:` e<sub>type</sub> )? `,` e<sub>dom</sub> n         | Pi declaration                     | [Pi](@ref thorin::Pi)       |
| d           | `.con` Sym p                       n                              | continuation declaration           | [Lam](@ref thorin::Lam)     |
| d           | `.fun` Sym p `→` e<sub>codom</sub> n                              | function declaration               | [Lam](@ref thorin::Lam)     |
| d           | `.lam` Sym p `→` e<sub>codom</sub> n                              | lambda declaration                 | [Lam](@ref thorin::Lam)     |
| d           | `.Arr` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n   | array declaration                  | [Arr](@ref thorin::Arr)     |
| d           | `.pack` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n  | pack declaration                   | [Pack](@ref thorin::Pack)   |
| d           | `.Sigma` Sym ( `:` e<sub>type</sub> )? `,` L<sub>arity</sub> v? n | sigma declaration                  | [Sigma](@ref thorin::Sigma) |
| d           | `.def` Sym n                                                      | mutable definition                 | mutables                    |
| n           | `;` \| o                                                          | mutable definition                 | -                           |
| o           | `=` de `;`                                                        | operand of definition              | -                           |
| o           | `=` `{` e `,` ... `,` e  `}` `;`                                  | operands of definition<sup>s</sup> | -                           |
<sup>s</sup> opens new scope

### Patterns

Patterns allow you to decompose a value into its components like in [Standard ML](https://en.wikibooks.org/wiki/Standard_ML_Programming/Types#Tuples) or other functional languages.
There are
* p: *parenthesis-style* patterns (`()`-style), and
* b: *bracket-style patterns* (`[]`-style) .

The main difference is that
* `(a, b, c)` means `(a: ?, b: ?, c: ?)` whereas
* `[a, b, c]` means `[_: a, _: c, _: d]`.

Note that you **can** switch from a `()`-style pattern to a `[]`-pattern but not vice versa.
For this reason there is no  rule for `()`-`[]`-pattern.
What is more, `()`-style patterns allow for *groups*:
* `(a b c: .Nat, d e: .Bool)` means `(a: .Nat, b: .Nat, c: .Nat, d: .Bool, e: .Bool)`.

Finally, you can introduce an optional name for the whole tuple pattern:
```
.let abc::(a, b, c) = (1, 2, 3);
```
This will bind
* `a` to `1`,
* `b` to `2`,
* `c` to `3`, and
* `abc` to `(1, 2, 3)`.

Finally, you can put a ``'`` in front of an identifier, to (potentially) rebind a name to a different value.
This is particularly useful, when dealing with memory:
```
.let 'lea = %mem.lea (arr_size, ‹arr_size; pb_type›, 0) (pb_arr, 0:I32);
.let 'mem = %mem.store (mem, lea, f);
.let 'lea = %mem.lea (arr_size, ‹arr_size; pb_type›, 0) (pb_arr, 1:I32);
.let 'mem = %mem.store (mem, lea, g);
```

| Nonterminal | Right-Hand Side                                                    | Comment                 |
|-------------|--------------------------------------------------------------------|-------------------------|
| p           | ``'``? Sym (`:` e<sub>type</sub> )?                                | identifier `()`-pattern |
| b           | (``'``? Sym `:`)? e<sub>type</sub>                                 | identifier `[]`-pattern |
| p           | (``'``? Sym `::`)? `(` g `,` ... `,` g `)` (`:` e<sub>type</sub>)? | `()`-`()`-tuple pattern |
| p           | (``'``? Sym `::`)? `[` b `,` ... `,` b `]` (`:` e<sub>type</sub>)? | `[]`-`()`-tuple pattern |
| b           | (``'``? Sym `::`)? `[` b `,` ... `,` b `]` (`:` e<sub>type</sub>)? | `[]`-`[]`-tuple pattern |
| g           | p                                                                  | group                   |
| g           | Sym+ `:` e                                                         | group                   |


### Expressions

| Nonterminal | Right-Hand Side                                                               | Comment                             | Thorin Class                    |
|-------------|-------------------------------------------------------------------------------|-------------------------------------|---------------------------------|
| de          | d\* e                                                                         | declaration expression              | -                               |
| e           | `.Univ`                                                                       | universise: type of a type level    | [Univ](@ref thorin::Univ)       |
| e           | `.Type` e                                                                     | type of level e                     | [Type](@ref thorin::Type)       |
| e           | `*`                                                                           | alias for `.Type (0:.Univ)`         | [Type](@ref thorin::Type)       |
| e           | `□`                                                                           | alias for `.Type (1:.Univ)`         | [Type](@ref thorin::Type)       |
| e           | `.Nat`                                                                        | natural number                      | [Nat](@ref thorin::Nat)         |
| e           | `.Idx`                                                                        | builtin of type `.Nat -> *`         | [Idx](@ref thorin::Idx)         |
| e           | `.Bool`                                                                       | alias for `.Idx 2`                  | [Idx](@ref thorin::Idx)         |
| e           | `{` de `}`                                                                    | block<sup>s</sup>                   | -                               |
| e           | L `:` e<sub>type</sub>                                                        | literal                             | [Lit](@ref thorin::Lit)         |
| e           | I<sub>n</sub>                                                                 | literal of type `.Idx n`            | [Lit](@ref thorin::Lit)         |
| e           | `.ff`                                                                         | alias for `0_2`                     | [Lit](@ref thorin::Lit)         |
| e           | `.tt`                                                                         | alias for `1_2`                     | [Lit](@ref thorin::Lit)         |
| e           | ( `.bot` \| `.top` ) ( `:` e<sub>type</sub> )?                                | bottom/top                          | [TExt](@ref thorin::TExt)       |
| e           | Sym                                                                           | identifier                          | -                               |
| e           | Ax                                                                            | use of an axiom                     | -                               |
| e           | e e                                                                           | application                         | [App](@ref thorin::App)         |
| e           | Sym `:` e<sub>dom</sub> `→` e<sub>codom</sub> `.` e<sub>body</sub>            | lambda<sup>s</sup>                  | [Lam](@ref thorin::Lam)         |
| d           | `.cn` Sym p                       `=` de                                      | continuation expression             | [Lam](@ref thorin::Lam)         |
| d           | `.fn` Sym p `→` e<sub>codom</sub> `=` de                                      | function expression                 | [Lam](@ref thorin::Lam)         |
| d           | `λ`   Sym p `→` e<sub>codom</sub> `=` de                                      | lambda expression                   | [Lam](@ref thorin::Lam)         |
| e           | e<sub>dom</sub> `→` e<sub>codom</sub>                                         | function type                       | [Pi](@ref thorin::Pi)           |
| e           | `Π` b `→` e<sub>codom</sub>                                                   | dependent function type<sup>s</sup> | [Pi](@ref thorin::Pi)           |
| e           | e `#` Sym                                                                     | extract via field "Sym"             | [Extract](@ref thorin::Extract) |
| e           | e `#` e<sub>index</sub>                                                       | extract                             | [Extract](@ref thorin::Extract) |
| e           | `.ins` `(` e<sub>tuple</sub> `,` e<sub>index</sub> `,` e<sub>value</sub> ` )` | insert                              | [Insert](@ref thorin::Insert)   |
| e           | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>` )` ( `:` e<sub>type</sub> )?   | tuple                               | [Tuple](@ref thorin::Tuple)     |
| e           | `[` b `,` ... `,` b `]`                                                       | sigma<sup>s</sup>                   | [Sigma](@ref thorin::Sigma)     |
| e           | `‹` i e<sub>shape</sub> `;` e<sub>body</sub>`›`                               | pack<sup>s</sup>                    | [Pack](@ref thorin::Pack)       |
| e           | `«` i e<sub>shape</sub> `;` e<sub>body</sub>`»`                               | array<sup>s</sup>                   | [Arr](@ref thorin::Arr)         |
<sup>s</sup> opens new scope

An elided type of
* a literal defaults to `.Nat`,
* a bottom/top defaults to `*`,
* a mutable defaults to `*`.

#### Precedence

Expressions nesting is disambiguated according to the following precedence table (from strongest to weakest binding):

| Operator             | Description                         | Associativity |
|----------------------|-------------------------------------|---------------|
| L `:` e              | type ascription of a literal        | -             |
| e `#` e              | extract                             | left-to-right |
| e e                  | application                         | left-to-right |
| `Π` Sym `:` e        | domain of a dependent function type | -             |
| `.fun` Sym Sym `:` e | mutable funciton declaration        | -             |
| `.lam` Sym Sym `:` e | mutable continuation declaration    | -             |
| `.fn` Sym `:` e      | mutable funciton expression         | -             |
| `.lm` Sym `:` e      | mutable continuation expression     | -             |
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

Named elements of mutable sigmas are avaiable for extracts/inserts.
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
