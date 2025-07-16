# Mim Language Reference {#langref}

[TOC]

## Notation

We use the following notation:

| Notation        | Meaning                                                                                            |
| --------------- | -------------------------------------------------------------------------------------------------- |
| `a`             | literally the terminal token `a`                                                                   |
| [`a``b`]        | matches `a` or `b`                                                                                 |
| [`a`-`c`]       | matches `a` - `c`                                                                                  |
| a\*             | zero or more repetitions of "a"                                                                    |
| a\+             | one or more repetitions of "a"                                                                     |
| a?              | "a" is optional                                                                                    |
| a `,` ... `,` a | `,`-separated list of zero or more "a" elements; may contain a trailing `,` at the end of the list |

## Lexical Structure {#lex}

Mim files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded and [lexed](https://en.wikipedia.org/wiki/Lexical_analysis) from left to right.

@note The [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy resolves any ambiguities in the lexical rules below.
For Example, `<<<` is lexed as `<<` and `<`.

### Terminals {#terminals}

The actual _[terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)_ are specified in the following tables.

#### Primary Terminals

The [grammatical rules](#grammar) will directly reference these _primary [terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)_.
_Secondary terminals_ are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the **same** lexical element as its corresponding _primary token_.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.
Both tokens are identified as `⊥`.

| Primary Terminals               | Secondary Terminals         | Comment                   |
| ------------------------------- | --------------------------- | ------------------------- |
| `(` `)` `[` `]` `{` `}`         |                             | delimiters                |
| `‹` `›` `«` `»`                 | `<<` `>>` `<` `>`           | UTF-8 delimiters          |
| `→` `⊥` `⊤` `★` `□` `λ`         | `->` `.bot` `.top` `*` `lm` | further UTF-8 tokens      |
| `=` `,` `;` `.` `#` `:` `%` `@` |                             | further tokens            |
| `<eof>`                         |                             | marks the end of the file |

In addition you can use `⟨`, `⟩`, `⟪`, and `⟫` as an alternative for `‹`, `›`, `«`, and `»`.

#### Keywords

In addition the following keywords are _terminals_:

| Terminal | Comment                                                  |
| -------- | -------------------------------------------------------- |
| `import` | imports another Mim file                                 |
| `plugin` | like `import` and additionally loads the compiler plugin |
| `axm`    | axiom                                                    |
| `let`    | let declaration                                          |
| `con`    | [continuation](@ref mim::Lam) declaration                |
| `fun`    | [function](@ref mim::Lam) declaration                    |
| `lam`    | [lambda](@ref mim::Lam) declaration                      |
| `ret`    | ret expression                                           |
| `cn`     | [continuation](@ref mim::Lam) expression                 |
| `fn`     | [function](@ref mim::Lam) expression                     |
| `lm`     | [lambda](@ref mim::Lam) expression                       |
| `Sigma`  | [Sigma](@ref mim::Sigma) declaration                     |
| `extern` | marks function as external                               |
| `ins`    | mim::Insert expression                                   |
| `insert` | alias for `ins`                                          |
| `Nat`    | mim::Nat                                                 |
| `Idx`    | mim::Idx                                                 |
| `Type`   | mim::Type                                                |
| `Univ`   | mim::Univ                                                |
| `ff`     | alias for `0₂`                                           |
| `tt`     | alias for `1₂`                                           |

| Terminal  | Alias           | Terminal | Alias     |
| --------- | --------------- | -------- | --------- |
| `tt` `ff` | `0₂` `1₂`       | `Bool`   | `Idx i1`  |
| `i1`      | `2`             | `I1`     | `Idx i1`  |
| `i8`      | `0x100`         | `I8`     | `Idx i8`  |
| `i16`     | `0x1'0000`      | `I16`    | `Idx i16` |
| `i32`     | `0x1'0000'0000` | `I32`    | `Idx i32` |
| `i64`     | `0`             | `I64`    | `Idx i64` |


#### Other Terminals

The following _terminals_ comprise more complicated patterns:

| Terminal      | Regular Expression                   | Comment                                                                                           |
| ------------- | ------------------------------------ | ------------------------------------------------------------------------------------------------- |
| 𝖨             | sym                                  | identifier                                                                                        |
| A             | `%` sym `.` sym (`.` sym)?           | [Annex](@ref mim::Annex) name                                                                     |
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
| X<sub>n</sub> | dec+ sub+<sub>n</sub>                | index literal of type `Idx n`                                                                     |
| X<sub>n</sub> | dec+ `_` dec+<sub>n</sub>            | index literal of type `Idx n`                                                                     |
| C             | <tt>'</tt>(a \| esc)<tt>'</tt>       | character literal; a ∊ [ASCII](https://en.wikipedia.org/wiki/ASCII) ∖ {`\`, <tt>'</tt>}           |
| S             | <tt>\"</tt>(a \| esc)\*<tt>\"</tt>   | string literal; a ∊ [ASCII](https://en.wikipedia.org/wiki/ASCII) ∖ {`\`, <tt>"</tt>}              |

The previous table resorts to the following definitions as shorthand:

| Name | Definition                                                            | Comment                                         |
| ---- | --------------------------------------------------------------------- | ----------------------------------------------- |
| 0b   | `0` \[ `b``B` \]                                                      | prefix for binary literals                      |
| 0o   | `0` \[ `o``O` \]                                                      | prefix for octal literals                       |
| 0x   | `0` \[ `x``X` \]                                                      | prefix for hexadecimal literals                 |
| bin  | \[ `0``1` \]                                                          | binary digit                                    |
| oct  | \[ `0`-`7` \]                                                         | octal digit                                     |
| dec  | \[ `0`-`9` \]                                                         | decimal digit                                   |
| sub  | \[ `₀`-`₉` \]                                                         | subscript digit (always decimal)                |
| hex  | \[ `0`-`9``a`-`f``A`-`F` \]                                           | hexadecimal digit                               |
| eE   | \[ `e` `E` \]                                                         | exponent in floating point literals             |
| pP   | \[ `p` `P` \]                                                         | exponent in floating point hexadecimal literals |
| sign | \[ `+` `-` \]                                                         |                                                 |
| sym  | \[ `_``a`-`z``A`-`Z` \]\[ `.``_``0`-`9``a`-`z``A`-`Z` \]\*            | symbol                                          |
| esc  | \[ <tt>\'</tt>`\"``\0``\a`<tt>\\b</tt>`\f``\n``\r`<tt>\\t</tt>`\v` \] | escape sequences                                |

So, _sym_ refers to the shorthand rule while _𝖨_ refers to the _terminal_ that is identical to _sym_.
However, the terminal _A_ also uses the shorthand rule _sym_.

### Comments

In addition, the following comments are available:

- `/* ... */`: multi-line comment
- `//`: single-line comment
- `///`: single-line comment for [Markdown](https://www.doxygen.nl/manual/markdown.html) [output](@ref cli):
  - Single-line `/// xxx` comments will put `xxx` directly into the Markdown output.
    You can put an optional space after the `///` that will be elided in the Markdown output.
  - Everything else will be put verbatim within a [fenced code block](https://www.doxygen.nl/manual/markdown.html#md_fenced).

## Grammar {#grammar}

Mim's grammar is defined as a [context-free grammar](https://en.wikipedia.org/wiki/Context-free_grammar) that consists of the _terminals_ defined [above](#terminals) as well as the _nonterminals_ and _productions_ defined below.
The start symbol is "m" (module).

The follwing tables summarizes the main nonterminals:

| Nonterminal | Meaning                         |
| ----------- | ------------------------------- |
| m           | [module](@ref module)           |
| d           | [declaration](@ref decl)        |
| p           | `()`-style [pattern](@ref ptrn) |
| b           | `[]`-style [pattern](@ref ptrn) |
| e           | [expression](@ref expr)         |

The following tables comprise all production rules:

### Module {#module}

| LHS | RHS            | Comment | MimIR Class              |
| --- | -------------- | ------- | ------------------------ |
| m   | dep\* d\*      | module  | [World](@ref mim::World) |
| dep | `import` S `;` | import  |                          |
| dep | `plugin` S `;` | plugin  |                          |

### Declarations {#decl}

| LHS | RHS                                                                                                                                               | Comment                              | MimIR Class                        |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------ | ---------------------------------- |
| d   | `let` (p \| A) `=` e `;`                                                                                                                          | let                                  | -                                  |
| d   | `lam` n p+ (`:` e<sub>codom</sub>)? ( `=` e)? `;`                                                                                                 | lambda declaration<sup>s</sup>       | [Lam](@ref mim::Lam)               |
| d   | `con` n p+ ( `=` e)? `;`                                                                                                                          | continuation declaration<sup>s</sup> | [Lam](@ref mim::Lam)               |
| d   | `fun` n p+ (`:` e<sub>ret</sub>)? ( `=` e)? `;`                                                                                                   | function declaration<sup>s</sup>     | [Lam](@ref mim::Lam)               |
| d   | `Sigma` n (`:` e<sub>type</sub> )? (`,` L<sub>arity</sub>)? (`=` b<sub>[ ]</sub>)? `;`                                                            | sigma declaration                    | [Sigma](@ref mim::Sigma)           |
| d   | `axm` A `:` e<sub>type</sub> (`(` sa `,` ... `,` sa `)`)? <br> (`,` 𝖨<sub>normalizer</sub>)? (`,` L<sub>curry</sub>)? (`,` L<sub>trip</sub>)? `;` | axiom                                | [Axiom](@ref mim::Axm)             |
| n   | 𝖨 \| A                                                                                                                                            | identifier or annex name             | `fe::Sym`/[Annex](@ref mim::Annex) |
| sa  | 𝖨 (`=` 𝖨 `,` ... `,` 𝖨)?                                                                                                                          | subtag with aliases                  |                                    |

@note An elided type of a `.Pi` or `Sigma` declaration defaults to `*`.

### Patterns {#ptrn}

Patterns allow you to decompose a value into its components like in [Standard ML](https://en.wikibooks.org/wiki/Standard_ML_Programming/Types#Tuples) or other functional languages.

| LHS | RHS                                   | Comment                             |
| --- | ------------------------------------- | ----------------------------------- |
| p   | 𝖨 (`:` e<sub>type</sub> )?            | identifier `()`-pattern             |
| p   | `(` (p \| g) `,` ... `,` (p \| g) `)` | `()`-`()`-tuple pattern<sup>s</sup> |
| p   | b                                     | `[]`-`()`-tuple pattern             |
| b   | 𝖨 (`:` e<sub>type</sub> )?            | identifier `[]`-pattern             |
| g   | 𝖨+ `:` e                              | group                               |

#### ()-style vs []-style

There are

- p: _parenthesis-style_ patterns (`()`-style), and
- b: _bracket-style patterns_ (`[]`-style) .

The main difference is that

- `(a, b, c)` means `(a: ?, b: ?, c: ?)` whereas
- `[a, b, c]` means `[_: a, _: c, _: d]` while
- `(a: A, b: B, c: C)` is the same as `[a: A, b: B, C: C]`.

You **can** switch from a `()`-style pattern to a `[]`-pattern but not vice versa.
For this reason there is no rule for a `()`-`[]`-pattern.

#### Groups

Tuple patterns allow for _groups_:

- `(a b c: Nat, d e: Bool)` means `(a: Nat, b: Nat, c: Nat, d: Bool, e: Bool)`.
- `[a b c: Nat, d e: Bool]` means `[a: Nat, b: Nat, c: Nat, d: Bool, e: Bool]`.

#### Alias Pattern

You can wrap a pattern into an _alias pattern_:

```rust
let (a, b, c) as abc = (1, 2, 3);
```

This will bind

- `a` to `1`,
- `b` to `2`,
- `c` to `3`, and
- `abc` to `(1, 2, 3)`.

Here is another example:

```rust
{T: *, a: Nat} as Ts → [%mem.M, %mem.Ptr Ts] → [%mem.M, T]
```

#### Rebind

A `let` and `ret` expression allows you to rebind the same name to a different value.
This is particularly useful, when dealing with memory:

```rust
let (mem, ptr) = %mem.alloc (I32, 0) mem;
let mem        = %mem.store (mem, ptr, 23:I32);
let (mem, val) = %mem.load (mem, ptr);
```

### Expressions {#expr}

#### Kinds & Builtin Types

| LHS | RHS      | Comment                        | MimIR Class            |
| --- | -------- | ------------------------------ | ---------------------- |
| e   | `Univ`   | universe: type of a type level | [Univ](@ref mim::Univ) |
| e   | `Type` e | type of level e                | [Type](@ref mim::Type) |
| e   | `*`      | alias for `Type (0:Univ)`      | [Type](@ref mim::Type) |
| e   | `□`      | alias for `Type (1:Univ)`      | [Type](@ref mim::Type) |
| e   | `Nat`    | natural number                 | [Nat](@ref mim::Nat)   |
| e   | `Idx`    | builtin of type `Nat → *`      | [Idx](@ref mim::Idx)   |
| e   | `Bool`   | alias for `Bool`               | [Idx](@ref mim::Idx)   |

#### Literals & Co

| LHS | RHS                         | Comment                        | MimIR Class                        |
| --- | --------------------------- | ------------------------------ | ---------------------------------- |
| e   | L (`:` e<sub>type</sub>)?   | literal                        | [Lit](@ref mim::Lit)               |
| e   | X<sub>n</sub>               | literal of type `Idx n`        | [Lit](@ref mim::Lit)               |
| e   | `ff`                        | alias for `0_2`                | [Lit](@ref mim::Lit)               |
| e   | `tt`                        | alias for `1_2`                | [Lit](@ref mim::Lit)               |
| e   | C                           | character literal of type `I8` | [Lit](@ref mim::Lit)               |
| e   | S                           | string tuple of type `«n; I8»` | [Tuple](@ref mim::Tuple)           |
| e   | `⊥` (`:` e<sub>type</sub>)? | bottom                         | [Bot](@ref mim::Bot)               |
| e   | `⊤` (`:` e<sub>type</sub>)? | top                            | [Top](@ref mim::Top)               |
| e   | n                           | identifier or annex name       | `fe::Sym`/[Annex](@ref mim::Annex) |
| e   | `{` d\* e `}`               | block<sup>s</sup>              | -                                  |

@note An elided type of

- a literal defaults to `Nat`,
- a bottom/top defaults to `*`.

#### Functions

| LHS | RHS                                    | Comment                                        | MimIR Class          |
| --- | -------------------------------------- | ---------------------------------------------- | -------------------- |
| e   | e<sub>dom</sub> `→` e<sub>codom</sub>  | function type                                  | [Pi](@ref mim::Pi)   |
| e   | b<sub>dom</sub> `→` e<sub>codom</sub>  | dependent function type<sup>s</sup>            | [Pi](@ref mim::Pi)   |
| e   | `Cn` b                                 | continuation type<sup>s</sup>                  | [Pi](@ref mim::Pi)   |
| e   | `Fn` b `→` e<sub>ret</sub>             | returning continuation type<sup>s</sup>        | [Pi](@ref mim::Pi)   |
| e   | `λ` p+ (`:` e<sub>codom</sub>)? `=` e  | lambda expression<sup>s</sup>                  | [Lam](@ref mim::Lam) |
| e   | `cn` p+ `=` e                          | continuation expression<sup>s</sup>            | [Lam](@ref mim::Lam) |
| e   | `fn` p+ (`:` e<sub>codom</sub>)? `=` e | function expression<sup>s</sup>                | [Lam](@ref mim::Lam) |
| e   | e e                                    | application                                    | [App](@ref mim::App) |
| e   | e `@` e                                | application making implicit arguments explicit | [App](@ref mim::App) |
| e   | `ret` p `=` e `$` e `;` d\* e          | ret expresison                                 | [App](@ref mim::App) |

#### Tuples

| LHS | RHS                                                                         | Comment               | MimIR Class                  |
| --- | --------------------------------------------------------------------------- | --------------------- | ---------------------------- |
| e   | b<sub>[ ]</sub>                                                             | sigma                 | [Sigma](@ref mim::Sigma)     |
| e   | `«` s `;` e<sub>body</sub>`»`                                               | array<sup>s</sup>     | [Arr](@ref mim::Arr)         |
| e   | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>`)` (`:` e<sub>type</sub>)?    | tuple                 | [Tuple](@ref mim::Tuple)     |
| e   | `‹` s `;` e<sub>body</sub>`›`                                               | pack<sup>s</sup>      | [Pack](@ref mim::Pack)       |
| e   | e `#` e<sub>index</sub>                                                     | extract               | [Extract](@ref mim::Extract) |
| e   | e `#` 𝖨                                                                     | extract via field "𝖨" | [Extract](@ref mim::Extract) |
| e   | `ins` `(` e<sub>tuple</sub> `,` e<sub>index</sub> `,` e<sub>value</sub> `)` | insert                | [Insert](@ref mim::Insert)   |
| s   | e<sub>shape</sub> \| S `:` e<sub>shape</sub>                                | shape                 | -                            |

### Precedence

Expressions nesting is disambiguated according to the following precedence table (from strongest to weakest binding):

| Level | Operator | Description                                    | Associativity |
| ----- | -------- | ---------------------------------------------- | ------------- |
| 1     | L `:` e  | type ascription of a literal                   | -             |
| 2     | e `#` e  | extract                                        | left-to-right |
| 3     | e e      | application                                    | left-to-right |
| 3     | e `@` e  | application making implicit arguments explicit | left-to-right |
| 4     | e `→` e  | function type                                  | right-to-left |

## Summary: Functions & Types

The following table summarizes the different tokens used for functions declarations, expressions, and types:

| Declaration | Expression    | Type |
| ----------- | ------------- | ---- |
| `lam`       | `lm` <br> `λ` | `->` |
| `con`       | `cn`          | `Cn` |
| `fun`       | `fn`          | `Fn` |

### Declarations

The following function _declarations_ are all equivalent:

```rust
lam f(T: *)((x y: T), return: T → ⊥)@(ff): ⊥ = return x;
con f(T: *)((x y: T), return: Cn T)          = return x;
fun f(T: *) (x y: T): T                       = return x;
```

Note that all partial evaluation filters default to `tt` except for `con`/`cn`/`fun`/`fn`.

### Expressions

The following function _expressions_ are all equivalent.
What is more, since they are bound by a _let declaration_, they have the exact same effect as the function _declarations_ above:

```rust
let f =  λ (T: *) ((x y: T), return: T → ⊥)@(ff): ⊥ = return x;
let f = lm (T: *) ((x y: T), return: T → ⊥)      : ⊥ = return x;
let f = cn (T: *) ((x y: T), return: Cn T)          = return x;
let f = fn (T: *)  (x y: T): T                       = return x;
```

### Applications

The following expressions for applying `f` are also equivalent:

```rust
f Nat ((23, 42), cn res: Nat = use(res))
ret res = f Nat $ (23, 42); use(res)
```

### Function Types

Finally, the following function types are all equivalent and denote the type of `f` above.

```rust
[T: *] →    [[T, T], T → ⊥] → ⊥
[T: *] → Cn [[T, T], Cn T]
[T: *] → Fn  [T, T] → T
```

## Scoping

Mim uses [_lexical scoping_](<https://en.wikipedia.org/wiki/Scope_(computer_science)#Lexical_scope>) where all names live within the same namespace - with a few exceptions noted below.
@note The grammar tables above also indiciate which constructs open new scopes (and close them again) with <b>this<sup>s</sup></b> annotation in the _Comments_ column.

### Underscore

The symbol `_` is special and never binds to an entity.
For this reason, `_` can be bound over and over again within the same scope (without effect).
Hence, using the symbol `_` will always result in a scoping error.

### Annex

Annex names are special and live in a global namespace.

### Field Names of Sigmas

Named elements of mutable sigmas are available for extracts/inserts.

@note
These names take precedence over the usual scope.
In the following example, `i` refers to the first element `i` of `X` and **not** to the `i` introduced via `let`:

```rust
let i = 1_2;
[i: Nat, j: Nat]::X → f X#i;
```

Use parentheses to refer to the `let`-bounded `i`:

```rust
let i = 1_2;
[i: Nat, j: Nat]::X → f X#(i);
```
