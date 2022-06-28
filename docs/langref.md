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

| Primary Terminals           | Secondary Terminals                        | Comment                   |
|-----------------------------|--------------------------------------------|---------------------------|
| `(` `)` `[` `]` `{` `}`     |                                            | delimiters                |
| `‹` `›` `«` `»`             | `<<` `>>` `<` `>`                          | UTF-8 delimiters          |
| `→` `⊥` `⊤` `★` `□` `λ` `Π` | `->` `.bot` `.top` `*` `\`  <tt>\|~\|</tt> | further UTF-8 tokens      |
| `=` `,` `;` `.` `#` `:` `@` |                                            | further tokens            |
| `<eof>`                     |                                            | marks the end of the file |

#### Keywords

In addition the following keywords are *terminals*:

| Terminal    | Comment                   |
|-------------|---------------------------|
| `.ax`       | axiom                     |
| `.let`      | let expression            |
| `.Pi`       | nominal Pi                |
| `.lam`      | nominal lam               |
| `.Arr`      | nominal Arr               |
| `.pack`     | nominal pack              |
| `.Sigma`    | nominal Sigma             |
| `.def`      | nominal definition        |
| `.external` | marks nominal as external |
| `.module`   | starts a module           |
| `.import`   | imports a dialect         |
| `.Nat`      | thorin::Nat               |
| `.ff`       | alias for `0₂`            |
| `.tt`       | alias for `1₂`            |

All keywords start with a `.` to prevent name clashes with identifiers.

#### Regular Expressions

The following *terminals* comprise more complicated patterns that are specified via [regular expressions](https://en.wikipedia.org/wiki/Regular_expression):

| Terminal | Regular Expression                   | Comment                                                                                           |
|----------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| Sym      | sym                                  | symbol                                                                                            |
| Ax       | `@` sym `.` sym ( `.` sym)?          | Axiom                                                                                             |
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
* `/* ... */` multi-line comment
* `//` single-line comment
* `///` single-line comment that is put into the Markdown output (see [Emitters](@ref emitters))

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

| Nonterminal | Right-Hand Side                                                   | New Scope? | Comment                        | Thorin Class  |
|-------------|-------------------------------------------------------------------|------------|--------------------------------|---------------|
| d           | `.ax` Ax `:` e<sub>type</sub> `;`                                 |            | axiom                          | thorin::Axiom |
| d           | `.let` Sym `:` e<sub>type</sub> `=` e `;`                         |            | let                            | -             |
| d           | `.Pi` Sym ( `:` e<sub>type</sub> )? `,` e<sub>dom</sub> n         |            | nominal Pi declaration         | thorin::Pi    |
| d           | `.lam` Sym `:` e<sub>type</sub> v? n                              |            | nominal lambda declaration     | thorin::Lam   |
| d           | `.Arr` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n   |            | nominal array declaration      | thorin::Arr   |
| d           | `.pack` Sym ( `:` e<sub>type</sub> )? `,` e<sub>shape</sub> v? n  |            | nominal pack declaration       | thorin::Pack  |
| d           | `.Sigma` Sym ( `:` e<sub>type</sub> )? `,` L<sub>arity</sub> v? n |            | nominal sigma declaration      | thorin::Sigma |
| d           | `.def` Sym n                                                      |            | nominal definition             | nominals      |
| v           | `,` `@` Sym \| `,` `@` `(` Sym `,` ... `,` Sym `)`                |            | nominal variable declaration   | nominals      |
| n           | `;` \| o                                                          |            | nominal definition             | -             |
| o           | `=` e `;`                                                         |            | operand of nominal definition  | -             |
| o           | `=` `{` e `,` ... `,` e  `}` `;`                                  | ✓          | operands of nominal definition | -             |

### Expressions

| Nonterminal | Right-Hand Side                                                             | New Scope? | Comment                             | Thorin Class    |
|-------------|-----------------------------------------------------------------------------|------------|-------------------------------------|-----------------|
| e           | `{` e `}`                                                                   | ✓          | block                               | -               |
| e           | `*`                                                                         |            | type                                | thorin::Type    |
| e           | L `:` e<sub>type</sub>                                                      |            | literal                             | thorin::Lit     |
| e           | ( `.bot` or `.top` ) ( `:` e<sub>type</sub> )?                              |            | bottom/top                          | thorin::TExt    |
| e           | Sym                                                                         |            | identifier                          | -               |
| e           | Ax                                                                          |            | use of an axiom                     | -               |
| e           | e e                                                                         |            | application                         | thorin::App     |
| e           | `λ` Sym `:` e<sub>dom</sub> `→` e<sub>codom</sub>  `.` e<sub>body</sub>     | ✓          | lambda                              | thorin::Lam     |
| e           | e<sub>dom</sub> `→` e<sub>codom</sub>                                       |            | function type                       | thorin::Pi      |
| e           | `Π` b e<sub>dom</sub> `→` e<sub>codom</sub>                                 | ✓          | dependent function type             | thorin::Pi      |
| e           | e `#` Sym                                                                   |            | extract via field "Sym"             | thorin::Extract |
| e           | e `#` e<sub>index</sub>                                                     |            | extract                             | thorin::Extract |
| e           | `.ins` `(` e `,` e `,` e ` )`                                               |            | insert                              | thorin::Insert  |
| e           | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>` )` ( `:` e<sub>type</sub> )? |            | tuple with optional type ascription | thorin::Tuple   |
| e           | `[` b e<sub>type 0</sub> `,` ... `,` b e<sub>type n-1</sub> `]`             | ✓          | sigma                               | thorin::Sigma   |
| e           | `‹` b e<sub>shape</sub> `;` e<sub>body</sub>`›`                             | ✓          | pack                                | thorin::Pack    |
| e           | `«` b e<sub>shape</sub> `;` e<sub>body</sub>`»`                             | ✓          | array                               | thorin::Arr     |
| e           | d e                                                                         |            | declaration                         | -               |
| b           | ( Sym `:` )?                                                                |            | optional binder                     | -               |

An elided type of
* a literal defaults to `.Nat`,
* a bottom/top defaults to `*`,
* a nominals defaults to `*`.

#### Precedence

Expressions nesting is disambiguated according to the following precedence table (from strongest to weakest binding):

| Operator      | Description                         | Associativity |
|---------------|-------------------------------------|---------------|
| L `:` e       | type ascription of a literal        | -             |
| e `#` e       | extract                             | left-to-right |
| e e           | application                         | left-to-right |
| `Π` Sym `:` e | domain of a dependent function type | -             |
| e `→` e       | function type                       | right-to-left |

Note that the domain of a dependent function type binds slightly stronger than `→`.
This has the effect that, e.g., `Π T: * → T -> T` has the expected binding like this: (`Π T: *`) `→` (`T → T`).
Otherwise, `→` would be consumed by the domain: `Π T:` (`* →` (`T → T`)) ↯.

## Scoping

Thorin uses [_lexical scoping_](https://en.wikipedia.org/wiki/Scope_(computer_science)#Lexical_scope) where all names live within the same namespace - with a few exceptions noted below.
The grammar tables above also indiciate which constructs open new scopes (and close them again).

### Underscore

The symbol `_` is special and never binds to an entity.
For this reason, `_` can be bound over and over again within the same scope (without effect).
Hence, using the symbol `_` will always result in a [scoping error](@ref thorin::ScopeError).

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
Π X: [i: .Nat, j: .Nat] -> f X#i;
```
Use parentheses to refer to the `.let`-bounded `i`:
```
.let i = 1_2;
Π X: [i: .Nat, j: .Nat] -> f X#(i);
```
