# Mim Language Reference {#langref}

[TOC]

## Notation

This document uses a lightweight [EBNF](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form)-style notation.

```ebnf
"a"        literal terminal token a
[a b]      one of a or b
[a-c]      character range from a to c
x*         zero or more repetitions of x
x+         one or more repetitions of x
x?         optional x
x ("," x)* ","?
           comma-separated list of zero or more x, with optional trailing comma
```

## Lexical Structure {#lex}

Mim source files are [UTF-8](https://en.wikipedia.org/wiki/UTF-8) encoded and are [lexed](https://en.wikipedia.org/wiki/Lexical_analysis) from left to right.

@note The lexer uses [maximal munch](https://en.wikipedia.org/wiki/Maximal_munch): ambiguities are resolved by taking the longest matching token.
For example, `<<<` is tokenized as `<<` followed by `<`.

### Terminals {#terminals}

The grammar refers to *primary terminals*. Some of them also have ASCII-only spellings, called *secondary terminals*, which denote the same lexical token.
For example, `λ` and `lm` are lexically equivalent.

#### Primary terminals

```text
( ) [ ] { }
‹ › « »
→ ⊥ ⊤ ★ □ λ
= , ; . # : % @
<eof>
```

#### Secondary terminals

```text
<< >> < >
-> .bot .top * lm
```

In addition, `⟨`, `⟩`, `⟪`, and `⟫` may be used as alternatives for `‹`, `›`, `«`, and `»`.

#### Keywords

```text
import plugin
axm let
con fun lam
ret cn fn lm
Sigma extern
ins insert
Nat Idx Type Univ
ff tt
Bool I1 I8 I16 I32 I64
i1 i8 i16 i32 i64
```

The following names are predefined aliases:

```text
tt   = 1₂
ff   = 0₂
Bool = Idx i1
I1   = Idx i1
I8   = Idx i8
I16  = Idx i16
I32  = Idx i32
I64  = Idx i64

i1   = 2
i8   = 0x100
i16  = 0x1'0000
i32  = 0x1'0000'0000
i64  = 0
```

#### Pattern terminals

The following terminals are defined by lexical patterns.

```ebnf
I      ::= sym
A      ::= "%" sym "." sym ("." sym)?
L      ::= dec+
         |  "0" ["bB"] bin+
         |  "0" ["oO"] oct+
         |  "0" ["xX"] hex+
         |  sign dec+
         |  sign "0" ["bB"] bin+
         |  sign "0" ["oO"] oct+
         |  sign "0" ["xX"] hex+
         |  sign? dec+ eE sign dec+
         |  sign? dec+ "." dec* (eE sign dec+)?
         |  sign? dec* "." dec+ (eE sign dec+)?
         |  sign? "0" ["xX"] hex+ pP sign dec+
         |  sign? "0" ["xX"] hex+ "." hex* pP sign dec+
         |  sign? "0" ["xX"] hex* "." hex+ pP sign dec+
X_n    ::= dec+ sub_n+
         |  dec+ "_" dec_n+
C      ::= "'" (ascii_char | esc) "'"
S      ::= "\"" (ascii_string_char | esc)* "\""
```

Here:

- `I` is an identifier terminal,
- `A` is an [annex](@ref mim::Annex) name,
- `L` is a numeric literal,
- `X_n` is an index literal of type `Idx n`,
- `C` is a character literal,
- `S` is a string literal.

The shorthand symbols used above are:

```ebnf
bin    ::= [0-1]
oct    ::= [0-7]
dec    ::= [0-9]
sub    ::= [₀-₉]
hex    ::= [0-9a-fA-F]
eE     ::= ["eE"]
pP     ::= ["pP"]
sign   ::= ["+-"]
sym    ::= [_a-zA-Z] [._0-9a-zA-Z]*
esc    ::= one of: \' \" \0 \a \\ \b \f \n \r \t \v
```

## Comments

Mim supports the following comment forms:

- `/* ... */` for multi-line comments
- `// ...` for single-line comments
- `/// ...` for [Markdown](https://www.doxygen.nl/manual/markdown.html) [output](@ref cli)

For `///` comments:

- a line of the form `/// text` contributes `text` directly to the Markdown output,
- other forms are emitted verbatim inside a [fenced code block](https://www.doxygen.nl/manual/markdown.html#md_fenced).

## Grammar {#grammar}

Mim is defined by a [context-free grammar](https://en.wikipedia.org/wiki/Context-free_grammar). Its terminals are the lexical elements defined above. The start symbol is `m` for *module*.

The main nonterminals are:

```text
m   module
d   declaration
p   ()-style pattern
b   []-style pattern
e   expression
```

### Module {#module}

```ebnf
m   ::= dep* d*
dep ::= "import" S ";"
     |  "plugin" S ";"
```

A module consists of zero or more imports/plugins followed by zero or more declarations.

### Declarations {#decl}

```ebnf
d   ::= "let" (p | A) "=" e ";"
     |  "lam" n p+ (":" e)? ("=" e)? ";"
     |  "con" n p+ ("=" e)? ";"
     |  "fun" n p+ (":" e)? ("=" e)? ";"
     |  "Sigma" n (":" e)? ("," L)? ("=" b)? ";"
     |  "axm" A ("(" sa ("," sa)* ","? ")")? ":" e
           ("," I)? ("," L)? ("," L)? ";"

n   ::= I | A
sa  ::= I ("=" I)*
```

Notes:

- `let` introduces a local binding.
- `lam`, `con`, and `fun` declare lambdas, continuations, and functions.
- `Sigma` declares a sigma type, optionally with an explicit type, arity, and field pattern.
- `axm` declares an axiom, optionally with subtags, normalizer, curry, and trip metadata.

@note If the type of a `Pi`-like or `Sigma` declaration is omitted, it defaults to `*`.

### Patterns {#ptrn}

Patterns allow values to be decomposed into components, similar to tuple patterns in functional languages.

```ebnf
p   ::= I (":" e)?
     |  "(" (pg ("," pg)* ","?)? ")"
     |  b

pg  ::= p
     |  g

b   ::= I (":" e)?
     |  "[" (bg ("," bg)* ","?)? "]"

bg  ::= b
     |  g

g   ::= I+ ":" e
```

#### ()-style vs []-style

There are two pattern families:

- `p`: parenthesis-style patterns
- `b`: bracket-style patterns

Intuitively:

- `(a, b, c)` means three bindings with inferred component types
- `[a, b, c]` means three bindings where the listed expressions describe the component types
- `(a: A, b: B, c: C)` and `[a: A, b: B, c: C]` coincide when all types are explicit

You may switch from `()`-style into `[]`-style, but not the other way around.

#### Groups

Tuple patterns support grouped bindings:

```rust
(a b c: Nat, d e: Bool)
[a b c: Nat, d e: Bool]
```

Both expand as expected by distributing the annotated type over the listed names.

#### Alias patterns

A pattern may be wrapped in an alias pattern:

```rust
let (a, b, c) as abc = (1, 2, 3);
```

This binds:

- `a` to `1`,
- `b` to `2`,
- `c` to `3`,
- `abc` to the whole tuple `(1, 2, 3)`.

For example:

```rust
{T: *, a: Nat} as Ts → [%mem.M a, %mem.Ptr Ts] → [%mem.M a, T]
```

#### Rebinding

`let` and `ret` expressions allow rebinding of the same name. This is especially useful in state-threading style code:

```rust
let (mem, ptr) = %mem.alloc (I32, 0) mem;
let mem        = %mem.store (mem, ptr, 23:I32);
let (mem, val) = %mem.load (mem, ptr);
```

### Expressions {#expr}

#### Kinds and builtin types

```ebnf
e   ::= "Univ"
     |  "Type" e
     |  "*"
     |  "□"
     |  "Nat"
     |  "Idx"
     |  "Bool"
```

Meaning:

- `Univ` is the universe of type levels,
- `Type e` is the type at level `e`,
- `*` abbreviates `Type (0:Univ)`,
- `□` abbreviates `Type (1:Univ)`,
- `Nat` is the natural number type,
- `Idx` is the builtin of type `Nat → *`,
- `Bool` abbreviates `Idx i1`.

#### Literals and basic forms

```ebnf
e   ::= L (":" e)?
     |  X_n
     |  "ff"
     |  "tt"
     |  C
     |  S
     |  "⊥" (":" e)?
     |  "⊤" (":" e)?
     |  n
     |  "{" d* e "}"
```

Notes:

- a literal without explicit type defaults to `Nat`,
- `⊥` and `⊤` default to type `*` when no type is given,
- a block introduces local declarations and yields its final expression.

#### Functions

```ebnf
e   ::= e "→" e
     |  b "→" e
     |  "Cn" b
     |  "Fn" b "→" e
     |  "λ" p+ (":" e)? "=" e
     |  "cn" p+ "=" e
     |  "fn" p+ (":" e)? "=" e
     |  e e
     |  e "@" e
     |  "ret" p "=" e "$" e ";" d* e
```

These forms cover:

- function types,
- dependent function types,
- continuation and function sugar,
- lambda/function/continuation expressions,
- application,
- explicit passing of implicit arguments,
- `ret` expressions.

#### Tuples, arrays, and products

```ebnf
e   ::= b
     |  "«" s ";" e "»"
     |  "(" (e ("," e)* ","?)? ")" (":" e)?
     |  "‹" s ";" e "›"
     |  e "#" e
     |  e "#" I
     |  "ins" "(" e "," e "," e ")"

s   ::= e
     |  S ":" e
```

These forms correspond to sigma types, arrays, tuples, packs, extracts, and inserts.

### Precedence

Expressions are parsed with the following precedence, from strongest to weakest binding:

```text
1.  L : e        literal type ascription
2.  e # e        extract
3.  e e          application
    e @ e        application with explicit implicit arguments
4.  e → e        function type
```

Application and extraction associate left-to-right. Function type associates right-to-left.

## Summary: Functions and types

Mim uses different surface syntax for declarations, expressions, and types:

```text
Declaration   Expression   Type
lam           lm / λ       ->
con           cn           Cn
fun           fn           Fn
```

### Declarations

The following declarations are equivalent:

```rust
lam f(T: *)((x y: T), return: T → ⊥)@ff: ⊥ = return x;
con f(T: *)((x y: T), return: Cn T)        = return x;
fun f(T: *) (x y: T): T                    = return x;
```

Partial-evaluation filters default to `tt`, except for `con`/`cn`/`fun`/`fn`.

### Expressions

The following expressions are equivalent. Because they are bound by `let`, they behave like the declarations above:

```rust
let f =  λ (T: *) ((x y: T), return: T → ⊥)@ff: ⊥ = return x;
let f = lm (T: *) ((x y: T), return: T → ⊥)   : ⊥ = return x;
let f = cn (T: *) ((x y: T), return: Cn T)        = return x;
let f = fn (T: *)  (x y: T): T                    = return x;
```

### Applications

The following applications of `f` are equivalent:

```rust
f Nat ((23, 42), cn res: Nat = use(res))
ret res = f Nat $ (23, 42); use(res)
```

### Function types

The following types are equivalent and describe the type of `f` above:

```rust
[T: *] →    [[T, T], T → ⊥] → ⊥
[T: *] → Cn [[T, T], Cn T]
[T: *] → Fn  [T, T] → T
```

## Scoping

Mim uses [lexical scoping](<https://en.wikipedia.org/wiki/Scope_(computer_science)#Lexical_scope>). Unless noted otherwise, all names live in the same namespace.

### Underscore

The symbol `_` is special: it never binds an entity.

As a consequence, `_` may appear repeatedly in the same scope without conflict, but any use of `_` as a reference is a scoping error.

### Annex

Annex names live in a separate global namespace.

### Field names of sigmas

Named elements of mutable sigma types are available for extracts and inserts.

@note These names take precedence over ordinary lexical names. In the example below, `i` refers to the field name of `X`, not the `let`-bound variable:

```rust
let i = 1_2;
[i: Nat, j: Nat]::X → f X#i;
```

Use parentheses to force the variable interpretation:

```rust
let i = 1_2;
[i: Nat, j: Nat]::X → f X#(i);
```
