# Language Reference {#langref}

[TOC]

## Notation

We use the following notation:

| Notation        | Meaning                                         |
|-----------------|-------------------------------------------------|
| `a`             | literally the terminal token `a`                |
| [`a``b`]        | matches `a` or `b`                              |
| [`a`-`c`]       | matches `a`- `c`                                |
| a\*             | zero or more repetitions of "a"                 |
| a\+             | one or more repetitions of "a"                  |
| a?              | "a" is optional                                 |
| a `,` ... `,` a | `,`-separated list of zero or more "a" elements |

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
| `.Pi`     | mutable [Pi](@ref thorin::Pi) declaration                 |
| `.let`    | let declaration                                           |
| `.con`    | [continuation](@ref thorin::Lam) declaration              |
| `.fun`    | [function](@ref thorin::Lam) declaration                  |
| `.lam`    | [lambda](@ref thorin::Lam) declaration                    |
| `.ret`    | ret expression                                            |
| `.cn`     | [continuation](@ref thorin::Lam) expression               |
| `.fn`     | [function](@ref thorin::Lam) expression                   |
| `.cn`     | [lambda](@ref thorin::Lam) expression                     |
| `.Sigma`  | mutable thorin::Sigma                                     |
| `.Pi`     | mutable thorin::Pi                                        |
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

#### Other Terminals.

The following *terminals* comprise more complicated patterns:

| Terminal      | Regular Expression                   | Comment                                                                                           |
|---------------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| Sym           | sym                                  | symbol                                                                                            |
| Ax            | `%` sym `.` sym (`.` sym)?           | Axiom                                                                                             |
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

| Name | Definition                                                 | Comment                                         |
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

| Nonterminal | Right-Hand Side   | Comment | Thorin Class                |
|-------------|-------------------|---------|-----------------------------|
| m           | l\* d\*           | module  | [World](@ref thorin::World) |
| l           | `.import` Sym `;` | import  |                             |
| l           | `.plugin` Sym `;` | plugin  |                             |

### Declarations

| Nonterminal | Right-Hand Side                                                                           | Comment                  | Thorin Class                |
|-------------|-------------------------------------------------------------------------------------------|--------------------------|-----------------------------|
| d           | `.ax` Ax `:` e<sub>type</sub> `;`                                                         | axiom                    | [Axiom](@ref thorin::Axiom) |
| d           | `.let` p  `=` e `;`                                                                       | let                      | -                           |
| d           | `.lam` Sym (`.`? p)+ `→` e<sub>codom</sub> ( `=` de)? `;`                                 | lambda declaration       | [Lam](@ref thorin::Lam)     |
| d           | `.con` Sym (`.`? p)+                       ( `=` de)? `;`                                 | continuation declaration | [Lam](@ref thorin::Lam)     |
| d           | `.fun` Sym (`.`? p)+ `→` e<sub>ret</sub>   ( `=` de)? `;`                                 | function declaration     | [Lam](@ref thorin::Lam)     |
| d           | `.Pi` Sym (`:` e<sub>type</sub>)? (`=` e)? `;`                                            | Pi declaration           | [Pi](@ref thorin::Pi)       |
| d           | `.Sigma` Sym (`:` e<sub>type</sub> )? (`,` L<sub>arity</sub>)? (`=` b<sub>[ ]</sub>)? `;` | sigma declaration        | [Sigma](@ref thorin::Sigma) |
<sup>s</sup> opens new scope

### Patterns

Patterns allow you to decompose a value into its components like in [Standard ML](https://en.wikibooks.org/wiki/Standard_ML_Programming/Types#Tuples) or other functional languages.
There are
* p: *parenthesis-style* patterns (`()`-style), and
* b: *bracket-style patterns* (`[]`-style) .

The main difference is that
* `(a, b, c)` means `(a: ?, b: ?, c: ?)` whereas
* `[a, b, c]` means `[_: a, _: c, _: d]` while
* `(a: A, b: B, c: C)` is the same as `[a: A, b: B, C: C]`.

Note that you **can** switch from a `()`-style pattern to a `[]`-pattern but not vice versa.
For this reason there is no rule for a `()`-`[]`-pattern.
What is more, `()`-style patterns allow for *groups*:
* `(a b c: .Nat, d e: .Bool)` means `(a: .Nat, b: .Nat, c: .Nat, d: .Bool, e: .Bool)`.

You can introduce an optional name for the whole tuple pattern:
```
.let abc::(a, b, c) = (1, 2, 3);
```
This will bind
* `a` to `1`,
* `b` to `2`,
* `c` to `3`, and
* `abc` to `(1, 2, 3)`.

Here is another example:
```
Π.Tas::[T: *, as: .Nat][%mem.M, %mem.Ptr Tas] -> [%mem.M, T]
```

Finally, you can put a <tt>\`</tt> in front of an identifier of a `()`-style pattern to (potentially) rebind a name to a different value.
This is particularly useful, when dealing with memory:
```
.let (`mem, ptr) = %mem.alloc (I32, 0) mem;
.let `mem        = %mem.store (mem, ptr, 23:I32);
.let (`mem, val) = %mem.load (mem, ptr);
```

| Nonterminal     | Right-Hand Side                            | Comment                 |
|-----------------|--------------------------------------------|-------------------------|
| p               | ``'``? Sym (`:` e<sub>type</sub> )?        | identifier `()`-pattern |
| p               | (``'``? Sym `::`)? `(` g `,` ... `,` g `)` | `()`-`()`-tuple pattern |
| p               | (``'``? Sym `::`)? b<sub>[ ]</sub>         | `[]`-`()`-tuple pattern |
| g               | p                                          | group                   |
| g               | Sym+ `:` e                                 | group                   |
| b               | (``'``? Sym `:`)? e<sub>type</sub>         | identifier `[]`-pattern |
| b               | (``'``? Sym `::`)? b<sub>[ ]</sub>         | `[]`-`[]`-tuple pattern |
| b<sub>[ ]</sub> | `[` b `,` ... `,` b `]`                    | `[]`-tuple pattern      |


### Expressions

| Nonterminal | Right-Hand Side                                                               | Comment                                 | Thorin Class                    |
|-------------|-------------------------------------------------------------------------------|-----------------------------------------|---------------------------------|
| de          | d\* e                                                                         | declarations, expression                | -                               |
| e           | `.Univ`                                                                       | universise: type of a type level        | [Univ](@ref thorin::Univ)       |
| e           | `.Type` e                                                                     | type of level e                         | [Type](@ref thorin::Type)       |
| e           | `*`                                                                           | alias for `.Type (0:.Univ)`             | [Type](@ref thorin::Type)       |
| e           | `□`                                                                           | alias for `.Type (1:.Univ)`             | [Type](@ref thorin::Type)       |
| e           | `.Nat`                                                                        | natural number                          | [Nat](@ref thorin::Nat)         |
| e           | `.Idx`                                                                        | builtin of type `.Nat -> *`             | [Idx](@ref thorin::Idx)         |
| e           | `.Bool`                                                                       | alias for `.Idx 2`                      | [Idx](@ref thorin::Idx)         |
| e           | `{` de `}`                                                                    | block<sup>s</sup>                       | -                               |
| e           | L (`:` e<sub>type</sub>)?                                                     | literal                                 | [Lit](@ref thorin::Lit)         |
| e           | I<sub>n</sub>                                                                 | literal of type `.Idx n`                | [Lit](@ref thorin::Lit)         |
| e           | `.ff`                                                                         | alias for `0_2`                         | [Lit](@ref thorin::Lit)         |
| e           | `.tt`                                                                         | alias for `1_2`                         | [Lit](@ref thorin::Lit)         |
| e           | (`.bot` \| `.top`) (`:` e<sub>type</sub>)?                                    | bottom/top                              | [TExt](@ref thorin::TExt)       |
| e           | Sym                                                                           | identifier                              | -                               |
| e           | Ax                                                                            | use of an axiom                         | -                               |
| e           | e e                                                                           | application                             | [App](@ref thorin::App)         |
| e           | `.ret` p `=` e `:` e `;` e                                                    | ret expresison                          | [App](@ref thorin::App)         |
| e           | `λ`   (`.`? p)+ (`→` e<sub>codom</sub>)? `=` de                               | lambda expression<sup>s</sup>           | [Lam](@ref thorin::Lam)         |
| e           | `.cn` (`.`? p)+                          `=` de                               | continuation expression<sup>s</sup>     | [Lam](@ref thorin::Lam)         |
| e           | `.fn` (`.`? p)+ (`→` e<sub>codom</sub>)? `=` de                               | function expression<sup>s</sup>         | [Lam](@ref thorin::Lam)         |
| e           | e<sub>dom</sub> `→` e<sub>codom</sub>                                         | function type                           | [Pi](@ref thorin::Pi)           |
| e           | `Π`   `.`? b (`.`? b<sub>[ ]</sub>)* `→` e<sub>codom</sub>                    | dependent function type<sup>s</sup>     | [Pi](@ref thorin::Pi)           |
| e           | `.Cn` `.`? b (`.`? b<sub>[ ]</sub>)*                                          | continuation type<sup>s</sup>           | [Pi](@ref thorin::Pi)           |
| e           | `.Fn` `.`? b (`.`? b<sub>[ ]</sub>)* `→` e<sub>codom</sub>                    | returning continuation type<sup>s</sup> | [Pi](@ref thorin::Pi)           |
| e           | e `#` Sym                                                                     | extract via field "Sym"                 | [Extract](@ref thorin::Extract) |
| e           | e `#` e<sub>index</sub>                                                       | extract                                 | [Extract](@ref thorin::Extract) |
| e           | `.ins` `(` e<sub>tuple</sub> `,` e<sub>index</sub> `,` e<sub>value</sub> ` )` | insert                                  | [Insert](@ref thorin::Insert)   |
| e           | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>` )` (`:` e<sub>type</sub>)?     | tuple                                   | [Tuple](@ref thorin::Tuple)     |
| e           | `[` b `,` ... `,` b `]`                                                       | sigma<sup>s</sup>                       | [Sigma](@ref thorin::Sigma)     |
| e           | `‹` i e<sub>shape</sub> `;` e<sub>body</sub>`›`                               | pack<sup>s</sup>                        | [Pack](@ref thorin::Pack)       |
| e           | `«` i e<sub>shape</sub> `;` e<sub>body</sub>`»`                               | array<sup>s</sup>                       | [Arr](@ref thorin::Arr)         |
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
| `.fun` Sym Sym `:` e | mutable function declaration        | -             |
| `.lam` Sym Sym `:` e | mutable continuation declaration    | -             |
| `.fn` Sym `:` e      | mutable function expression         | -             |
| `.lm` Sym `:` e      | mutable continuation expression     | -             |
| e `→` e              | function type                       | right-to-left |

Note that the domain of a dependent function type binds slightly stronger than `→`.
This has the effect that, e.g., `Π T: * → T → T` has the expected binding like this: (`Π T: *`) `→` (`T → T`).
Otherwise, `→` would be consumed by the domain: `Π T:` (`* →` (`T → T`)) ↯.
A similar situation occurs for a `.lam` declaration.

### Functions \& Types

The following table summarizes the different tokens used for functions declarations, expressions, and types:

| Declaration | Expression     | Type                    |
|-------------|----------------|-------------------------|
| `.lam`      | `.lm` <br> `λ` | `Π` <br> <tt>\|~\|</tt> |
| `.con`      | `.cn`          | `.Cn`                   |
| `.fun`      | `.fn`          | `.Fn`                   |

#### Declarations

The following function *declarations* are all equivalent:
```
.lam f(T: *)((x y: T), return: T -> ⊥) -> ⊥ = return x;
.con f(T: *)((x y: T), return: .Cn T)       = return x;
.fun f(T: *) (x y: T)                       = return x;
```

#### Expressions

The following function *expressions* are all equivalent.
What is more, since they are bound by a *let declaration*, they have the exact same effect as the function *declarations* above:
```
.let f = .lm (T: *)((x y: T), return: T -> ⊥) -> ⊥ = return x;
.let f = .cn (T: *)((x y: T), return: .Cn T)       = return x;
.let f = .fn (T: *) (x y: T)                       = return x;
```

#### Applications

The following expressions for applying `f` are also equivalent:
```
f .Nat ((23, 42),.cn res: .Nat = use(res))
.ret res = f .Nat : (23, 42); use(res)
```
#### Function Types

Finally, the following function types are all equivalent and denote the type of `f` above.
```
 Π [T:*][T, T][T -> ⊥] -> ⊥
.Cn[T:*][T, T][.Cn T]
.Fn[T:*][T, T] -> T
```

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
