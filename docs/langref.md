# Language Reference {#langref}

[TOC]

## Notation

We use the following notation:

| Notation        | Meaning                                                                                            |
|-----------------|----------------------------------------------------------------------------------------------------|
| `a`             | literally the terminal token `a`                                                                   |
| [`a``b`]        | matches `a` or `b`                                                                                 |
| [`a`-`c`]       | matches `a` - `c`                                                                                  |
| a\*             | zero or more repetitions of "a"                                                                    |
| a\+             | one or more repetitions of "a"                                                                     |
| a?              | "a" is optional                                                                                    |
| a `,` ... `,` a | `,`-separated list of zero or more "a" elements; may contain a trailing `,` at the end of the list |

## Lexical Structure {#lex}

Thorin files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded and [lexed](https://en.wikipedia.org/wiki/Lexical_analysis) from left to right.

@note The [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch) strategy resolves any ambiguities in the lexical rules below.
For Example, `<<<` is lexed as `<<` and `<`.

### Terminals {#terminals}

The actual *[terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)* are specified in the following tables.

#### Primary Terminals

The [grammatical rules](#grammar) will directly reference these *primary [terminals](https://en.wikipedia.org/wiki/Terminal_and_nonterminal_symbols)*.
*Secondary terminals* are [ASCII](https://en.wikipedia.org/wiki/ASCII)-only tokens that represent the **same** lexical element as its corresponding *primary token*.
For example, the lexer doesn't care, if you use `⊥` or `.bot`.
Both tokens are identified as `⊥`.

| Primary Terminals               | Secondary Terminals                         | Comment                   |
|---------------------------------|---------------------------------------------|---------------------------|
| `(` `)` `[` `]` `{` `}`         |                                             | delimiters                |
| `‹` `›` `«` `»`                 | `<<` `>>` `<` `>`                           | UTF-8 delimiters          |
| `→` `⊥` `⊤` `★` `□` `λ` `Π`     | `->` `.bot` `.top` `*` `.lm` <tt>\|~\|</tt> | further UTF-8 tokens      |
| `=` `,` `;` `.` `#` `:` `%` `@` |                                             | further tokens            |
| `<eof>`                         |                                             | marks the end of the file |

In addition you can use `⟨`, `⟩`, `⟪`, and `⟫` as an alternative for `‹`, `›`, `«`, and `»`.

#### Keywords

In addition the following keywords are *terminals*:

| Terminal  | Comment                                                   |
|-----------|-----------------------------------------------------------|
| `.module` | starts a module                                           |
| `.import` | imports another Thorin file                               |
| `.plugin` | like `.import` and additionally loads the compiler plugin |
| `.ax`     | axiom                                                     |
| `.let`    | let declaration                                           |
| `.con`    | [continuation](@ref thorin::Lam) declaration              |
| `.fun`    | [function](@ref thorin::Lam) declaration                  |
| `.lam`    | [lambda](@ref thorin::Lam) declaration                    |
| `.ret`    | ret expression                                            |
| `.cn`     | [continuation](@ref thorin::Lam) expression               |
| `.fn`     | [function](@ref thorin::Lam) expression                   |
| `.lm`     | [lambda](@ref thorin::Lam) expression                     |
| `.Pi`     | [Pi](@ref thorin::Pi) declaration                         |
| `.Sigma`  | [Sigma](@ref thorin::Sigma) declaration                   |
| `.extern` | marks function as external                                |
| `.ins`    | thorin::Insert expression                                 |
| `.insert` | alias for `.ins`                                          |
| `.Nat`    | thorin::Nat                                               |
| `.Idx`    | thorin::Idx                                               |
| `.Bool`   | alias for `.Idx 2`                                        |
| `.ff`     | alias for `0₂`                                            |
| `.tt`     | alias for `1₂`                                            |
| `.Type`   | thorin::Type                                              |
| `.Univ`   | thorin::Univ                                              |

All keywords start with a `.` to prevent name clashes with identifiers.

#### Other Terminals

The following *terminals* comprise more complicated patterns:

| Terminal      | Regular Expression                   | Comment                                                                                           |
|---------------|--------------------------------------|---------------------------------------------------------------------------------------------------|
| 𝖨             | sym                                  | identifier                                                                                        |
| A             | `%` sym `.` sym (`.` sym)?           | [Annex](@ref thorin::Annex) name                                                                  |
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
| X<sub>n</sub> | dec+ sub+<sub>n</sub>                | index literal of type `.Idx n`                                                                    |
| X<sub>n</sub> | dec+ `_` dec+<sub>n</sub>            | index literal of type `.Idx n`                                                                    |
| C             | <tt>'</tt>(a \| esc)<tt>'</tt>       | character literal; a ∊ [ASCII](https://en.wikipedia.org/wiki/ASCII) ∖ {`\`, <tt>'</tt>}           |
| S             | <tt>\"</tt>(a \| esc)*<tt>\"</tt>    | string literal; a ∊ [ASCII](https://en.wikipedia.org/wiki/ASCII) ∖ {`\`, <tt>"</tt>}              |

The previous table resorts to the following definitions as shorthand:

| Name | Definition                                                            | Comment                                         |
|------|-----------------------------------------------------------------------|-------------------------------------------------|
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

So, *sym* refers to the shorthand rule while *𝖨* refers to the *terminal* that is identical to *sym*.
However, the terminal *A* also uses the shorthand rule *sym*.

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

The follwing tables summarizes the main nonterminals:

| Nonterminal | Meaning                         |
|-------------|---------------------------------|
| m           | [module](@ref module)           |
| d           | [declaration](@ref decl)        |
| p           | `()`-style [pattern](@ref ptrn) |
| b           | `[]`-style [pattern](@ref ptrn) |
| e           | [expression](@ref expr)         |

The following tables comprise all production rules:

### Module {#module}

| LHS | RHS             | Comment | Thorin Class                |
|-----|-----------------|---------|-----------------------------|
| m   | dep\* d\*       | module  | [World](@ref thorin::World) |
| dep | `.import` S `;` | import  |                             |
| dep | `.plugin` S `;` | plugin  |                             |

### Declarations {#decl}

| LHS | RHS                                                                                                                                                  | Comment                              | Thorin Class                                        |
|-----|------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------|-----------------------------------------------------|
| d   | `.let`   (p \| A)  `=` e `;`                                                                                                                         | let                                  | -                                                   |
| d   | `.lam`   n (`.`? p)+ `→` e<sub>codom</sub> ( `=` d\* e)? `;`                                                                                         | lambda declaration<sup>s</sup>       | [Lam](@ref thorin::Lam)                             |
| d   | `.con`   n (`.`? p)+                       ( `=` d\* e)? `;`                                                                                         | continuation declaration<sup>s</sup> | [Lam](@ref thorin::Lam)                             |
| d   | `.fun`   n (`.`? p)+ `→` e<sub>ret</sub>   ( `=` d\* e)? `;`                                                                                         | function declaration<sup>s</sup>     | [Lam](@ref thorin::Lam)                             |
| d   | `.Pi`    n (`:` e<sub>type</sub>)? (`=` e)? `;`                                                                                                      | Pi declaration                       | [Pi](@ref thorin::Pi)                               |
| d   | `.Sigma` n (`:` e<sub>type</sub> )? (`,` L<sub>arity</sub>)? (`=` b<sub>[ ]</sub>)? `;`                                                              | sigma declaration                    | [Sigma](@ref thorin::Sigma)                         |
| d   | `.ax`    A `:` e<sub>type</sub> (`(` sa `,` ... `,` sa `)`)? <br> (`,` 𝖨<sub>normalizer</sub>)? (`,` L<sub>curry</sub>)? (`,` L<sub>trip</sub>)? `;` | axiom                                | [Axiom](@ref thorin::Axiom)                         |
| n   | 𝖨 \| A                                                                                                                                               | identifier or annex name             | [Sym](@ref thorin::Sym)/[Annex](@ref thorin::Annex) |
| sa  | 𝖨 (`=` 𝖨 `,` ... `,` 𝖨)?                                                                                                                             | subtag with aliases                  |                                                     |

@note An elided type of a `.Pi` or `.Sigma` declaration defaults to `*`.

### Patterns {#ptrn}

Patterns allow you to decompose a value into its components like in [Standard ML](https://en.wikibooks.org/wiki/Standard_ML_Programming/Types#Tuples) or other functional languages.

| LHS             | RHS                                                    | Comment                             |
|-----------------|--------------------------------------------------------|-------------------------------------|
| p               | <tt>\`</tt>? 𝖨 (`:` e<sub>type</sub> )?                | identifier `()`-pattern             |
| p               | (<tt>\`</tt>? 𝖨 `::`)? `(` d\* g `,` ... `,` d\* g `)` | `()`-`()`-tuple pattern<sup>s</sup> |
| p               | (<tt>\`</tt>? 𝖨 `::`)? b<sub>[ ]</sub>                 | `[]`-`()`-tuple pattern             |
| g               | p                                                      | group                               |
| g               | 𝖨+ `:` e                                               | group                               |
| b               | (<tt>\`</tt>? 𝖨 `:`)? e<sub>type</sub>                 | identifier `[]`-pattern             |
| b               | (<tt>\`</tt>? 𝖨 `::`)? b<sub>[ ]</sub>                 | `[]`-`[]`-tuple pattern             |
| b<sub>[ ]</sub> | `[` d\* b `,` ... `,` d\* b `]`                        | `[]`-tuple pattern<sup>s</sup>      |

#### ()-style vs []-style

There are
* p: *parenthesis-style* patterns (`()`-style), and
* b: *bracket-style patterns* (`[]`-style) .

The main difference is that
* `(a, b, c)` means `(a: ?, b: ?, c: ?)` whereas
* `[a, b, c]` means `[_: a, _: c, _: d]` while
* `(a: A, b: B, c: C)` is the same as `[a: A, b: B, C: C]`.

You **can** switch from a `()`-style pattern to a `[]`-pattern but not vice versa.
For this reason there is no rule for a `()`-`[]`-pattern.

#### Groups

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
Π.Tas::[T: *, as: .Nat][%mem.M, %mem.Ptr Tas] → [%mem.M, T]
```

#### Rebind

Finally, you can put a <tt>\`</tt> in front of an identifier of a `()`-style pattern to (potentially) rebind a name to a different value.
This is particularly useful, when dealing with memory:
```
.let (`mem, ptr) = %mem.alloc (I32, 0) mem;
.let `mem        = %mem.store (mem, ptr, 23:I32);
.let (`mem, val) = %mem.load (mem, ptr);
```

### Expressions {#expr}

#### Kinds & Builtin Types

| LHS | RHS       | Comment                          | Thorin Class              |
|-----|-----------|----------------------------------|---------------------------|
| e   | `.Univ`   | universise: type of a type level | [Univ](@ref thorin::Univ) |
| e   | `.Type` e | type of level e                  | [Type](@ref thorin::Type) |
| e   | `*`       | alias for `.Type (0:.Univ)`      | [Type](@ref thorin::Type) |
| e   | `□`       | alias for `.Type (1:.Univ)`      | [Type](@ref thorin::Type) |
| e   | `.Nat`    | natural number                   | [Nat](@ref thorin::Nat)   |
| e   | `.Idx`    | builtin of type `.Nat → *`       | [Idx](@ref thorin::Idx)   |
| e   | `.Bool`   | alias for `.Idx 2`               | [Idx](@ref thorin::Idx)   |

#### Literals & Co.

| LHS | RHS                         | Comment                              | Thorin Class                                        |
|-----|-----------------------------|--------------------------------------|-----------------------------------------------------|
| e   | L (`:` e<sub>type</sub>)?   | literal                              | [Lit](@ref thorin::Lit)                             |
| e   | X<sub>n</sub>               | literal of type `.Idx n`             | [Lit](@ref thorin::Lit)                             |
| e   | `.ff`                       | alias for `0_2`                      | [Lit](@ref thorin::Lit)                             |
| e   | `.tt`                       | alias for `1_2`                      | [Lit](@ref thorin::Lit)                             |
| e   | C                           | character literal of type `.Idx 256` | [Lit](@ref thorin::Lit)                             |
| e   | S                           | string tuple of type `«n; .Idx 256»` | [Tuple](@ref thorin::Tuple)                         |
| e   | `⊥` (`:` e<sub>type</sub>)? | bottom                               | [Bot](@ref thorin::Bot)                             |
| e   | `⊤` (`:` e<sub>type</sub>)? | top                                  | [Top](@ref thorin::Top)                             |
| e   | n                           | identifier or annex name             | [Sym](@ref thorin::Sym)/[Annex](@ref thorin::Annex) |
| e   | `{` d\* e `}`               | block<sup>s</sup>                    | -                                                   |

@note An elided type of
* a literal defaults to `.Nat`,
* a bottom/top defaults to `*`.

#### Functions

| LHS | RHS                                                         | Comment                                        | Thorin Class            |
|-----|-------------------------------------------------------------|------------------------------------------------|-------------------------|
| e   | e<sub>dom</sub> `→` e<sub>codom</sub>                       | function type                                  | [Pi](@ref thorin::Pi)   |
| e   | `Π`   `.`? b (`.`? b<sub>[ ]</sub>)\* `→` e<sub>codom</sub> | dependent function type<sup>s</sup>            | [Pi](@ref thorin::Pi)   |
| e   | `.Cn` `.`? b (`.`? b<sub>[ ]</sub>)\*                       | continuation type<sup>s</sup>                  | [Pi](@ref thorin::Pi)   |
| e   | `.Fn` `.`? b (`.`? b<sub>[ ]</sub>)\* `→` e<sub>codom</sub> | returning continuation type<sup>s</sup>        | [Pi](@ref thorin::Pi)   |
| e   | `λ`   (`.`? p)+ (`→` e<sub>codom</sub>)? `=` d\* e          | lambda expression<sup>s</sup>                  | [Lam](@ref thorin::Lam) |
| e   | `.cn` (`.`? p)+                          `=` d\* e          | continuation expression<sup>s</sup>            | [Lam](@ref thorin::Lam) |
| e   | `.fn` (`.`? p)+ (`→` e<sub>codom</sub>)? `=` d\* e          | function expression<sup>s</sup>                | [Lam](@ref thorin::Lam) |
| e   | e e                                                         | application                                    | [App](@ref thorin::App) |
| e   | e `@` e                                                     | application making implicit arguments explicit | [App](@ref thorin::App) |
| e   | `.ret` p `=` e `$` e `;` d\* e                              | ret expresison                                 | [App](@ref thorin::App) |

#### Tuples

| LHS | RHS                                                                           | Comment               | Thorin Class                    |
|-----|-------------------------------------------------------------------------------|-----------------------|---------------------------------|
| e   | b<sub>[ ]</sub>                                                               | sigma                 | [Sigma](@ref thorin::Sigma)     |
| e   | `«` s `;` e<sub>body</sub>`»`                                                 | array<sup>s</sup>     | [Arr](@ref thorin::Arr)         |
| e   | `(` e<sub>0</sub> `,` ... `,` e<sub>n-1</sub>` )` (`:` e<sub>type</sub>)?     | tuple                 | [Tuple](@ref thorin::Tuple)     |
| e   | `‹` s `;` e<sub>body</sub>`›`                                                 | pack<sup>s</sup>      | [Pack](@ref thorin::Pack)       |
| e   | e `#` e<sub>index</sub>                                                       | extract               | [Extract](@ref thorin::Extract) |
| e   | e `#` 𝖨                                                                       | extract via field "𝖨" | [Extract](@ref thorin::Extract) |
| e   | `.ins` `(` e<sub>tuple</sub> `,` e<sub>index</sub> `,` e<sub>value</sub> ` )` | insert                | [Insert](@ref thorin::Insert)   |
| s   | e<sub>shape</sub> \| S `:` e<sub>shape</sub>                                  | shape                 | -                               |

### Precedence

Expressions nesting is disambiguated according to the following precedence table (from strongest to weakest binding):

| Level | Operator   | Description                                    | Associativity |
|-------|------------|------------------------------------------------|---------------|
| 1     | L `:` e    | type ascription of a literal                   | -             |
| 2     | e `#` e    | extract                                        | left-to-right |
| 3     | e e        | application                                    | left-to-right |
| 3     | e `@` e    | application making implicit arguments explicit | left-to-right |
| 4     | `Π` b      | domain of a dependent function type            | -             |
| 5     | `.fun` n p | function declaration                           | -             |
| 5     | `.lam` n p | lambda declaration                             | -             |
| 5     | `.fn` p    | function expression                            | -             |
| 5     | `λ` p      | lambda expression                              | -             |
| 6     | e `→` e    | function type                                  | right-to-left |

@note The domain of a dependent function type binds slightly stronger than `→`.
This has the effect that
```
Π T: * → T → T
```
has the expected binding like this:
```
(Π T: *) → (T → T)
```
Otherwise, `→` would be consumed by the domain:
```
Π T: (* → (T → T)) ↯
```
A similar situation occurs for a `.lam` declaration.

## Summary: Functions & Types

The following table summarizes the different tokens used for functions declarations, expressions, and types:

| Declaration | Expression     | Type                    |
|-------------|----------------|-------------------------|
| `.lam`      | `.lm` <br> `λ` | <tt>\|~\|</tt> <br> `Π` |
| `.con`      | `.cn`          | `.Cn`                   |
| `.fun`      | `.fn`          | `.Fn`                   |

### Declarations

The following function *declarations* are all equivalent:
```
.lam f(T: *)((x y: T), return: T → ⊥) → ⊥ = return x;
.con f(T: *)((x y: T), return: .Cn T)     = return x;
.fun f(T: *) (x y: T) → T                 = return x;
```

### Expressions

The following function *expressions* are all equivalent.
What is more, since they are bound by a *let declaration*, they have the exact same effect as the function *declarations* above:
```
.let f =   λ (T: *)((x y: T), return: T → ⊥) → ⊥ = return x;
.let f = .lm (T: *)((x y: T), return: T → ⊥) → ⊥ = return x;
.let f = .cn (T: *)((x y: T), return: .Cn T)     = return x;
.let f = .fn (T: *) (x y: T) → T                 = return x;
```

### Applications

The following expressions for applying `f` are also equivalent:
```
f .Nat ((23, 42), .cn res: .Nat = use(res))
.ret res = f .Nat $ (23, 42); use(res)
```

### Function Types

Finally, the following function types are all equivalent and denote the type of `f` above.
```
 Π  [T:*][[T, T], T → ⊥] → ⊥
.Cn [T:*][[T, T], .Cn T]
.Fn [T:*] [T, T] → T
```

## Scoping

Thorin uses [_lexical scoping_](https://en.wikipedia.org/wiki/Scope_(computer_science)#Lexical_scope) where all names live within the same namespace - with a few exceptions noted below.
@note The grammar tables above also indiciate which constructs open new scopes (and close them again) with <b>this<sup>s</sup></b> annotation in the *Comments* column.

### Underscore

The symbol `_` is special and never binds to an entity.
For this reason, `_` can be bound over and over again within the same scope (without effect).
Hence, using the symbol `_` will always result in a scoping error.

### Pis

@note **Only**
```
Π x: e → e
```
introduces a new scope whereas
```
x: e → e
```
is a syntax error.
If the variable name of a Pi's domain is elided and the domain is a sigma, its elements will be imported into the Pi's scope to make these elements available in the Pi's codomain:
```
Π [T: *, U: *] → [T, U]
```

### Annex

Annex names are special and live in a global namespace.

### Field Names of Sigmas

Named elements of mutable sigmas are available for extracts/inserts.

@note
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
