# Developer Guide {#dev}

[TOC]

This guide summaries typical idioms you want to use when working with MimIR as a developer.

## Basics

Let's jump straight into an example.
\include "examples/hello.cpp"

[Driver](@ref mim::Driver) is the first class that you want to allocate.
It keeps track of a few "global" variables like some [Flags](@ref mim::Flags) or the [Log](@ref mim::Log).
Here, the log is set up to output to `std::cerr` with mim::Log::Level::Debug (see also @ref clidebug).

Then, we load the plugins [compile](@ref compile) and [core](@ref core), which in turn will load the plugin [mem](@ref mem).
A plugin consists of a shared object (`.so`/`.dll`) and a `.mim` file.
The shared object contains [Passes](@ref mim::Pass), [normalizers](@ref mim::Axiom::normalizer), and so on.
The `.mim` file contains the [axiom](@ref mim::Axiom) declarations and links the normalizers with the [axioms](@ref mim::Axiom).
For this reason, we need to allocate the mim::fe::Parser to parse the `.mim` file; mim::fe::Parser::plugin will also load the shared object.
The [driver](@ref mim::Driver) keeps track of all plugins.

Next, we create actual code.
[Def](@ref mim::Def) is the base class for **all** nodes/expressions in MimIR.
Each [Def](@ref mim::Def) is a node in a graph which is maintained in the [World](@ref mim::World).
The [World](@ref mim::World) is essentially a big hash set where all [Defs](@ref mim::Def) are tossed into.
The [World](@ref mim::World) provides factory methods to create all kind of different [Defs](@ref mim::Def).
Here, we create the `main` function.
In direct style, its type looks like this:

```
[%mem.M, I32, %mem.Ptr (I32, 0)] -> [%mem.M, I32]]
```

Converted to [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style) this type looks like this:

```
Cn [%mem.M, I32, %mem.Ptr (I32, 0), Cn [%mem.M, I32]]
```

The `%%mem.M` type is a type that keeps track of side effects that may occur.
Since, `main` introduces [Var](@ref mim::Var)iables we must create a **mutable** [Lam](@ref mim::Lam)bda (see @ref mut).
The only thing `main` is doing, is to invoke its `ret`urn continuation with `mem` and `argc` as argument:

```
ret (mem, argc)
```

It is also important to make `main` [external](@ref mim::Def::make_external).
Otherwise, MimIR will simply remove this function.

We [optimize](@ref mim::optimize) the program, emit an [LLVM assembly file](https://llvm.org/docs/LangRef.html), and compile it [via](@ref mim::sys::system) `clang`.
Finally, we [execute](@ref mim::sys::system) the generated program with `./hello a b c` and [output](@ref fmt) its exit code - which should be `4`.

## Immutables vs. Mutables {#mut}

There are two different kind of [Defs](@ref mim::Def) in MimIR: _mutables_ and _immutables_:

| **Immutable**                                                        | **Mutable**                                                         |
| -------------------------------------------------------------------- | ------------------------------------------------------------------- |
| _must be_ `const`                                                    | _may be_ **non**-`const`                                            |
| ops form [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph) | ops may be cyclic                                                   |
| no recursion                                                         | may be recursive                                                    |
| no [Var](@ref mim::Var)                                              | has [Var](@ref mim::Var); get with [Def::var](@ref mim::Def::var)   |
| build ops first, then the actual node                                | build the actual node first, then [set](@ref mim::Def::set) the ops |
| [Hash consed](https://en.wikipedia.org/wiki/Hash_consing)            | each new instance is fresh                                          |
| [Def::rebuild](@ref mim::Def::rebuild)                               | [Def::stub](@ref mim::Def::stub)                                    |

## Matching IR

MimIR provides different means to scrutinize [Defs](@ref mim::Def).
Usually, you will encounter a [Def](@ref mim::Def) as [Ref](@ref mim::Ref) which is just a wrapper for a `const Def*`.
Its purpose is to resolve "holes" (called _[Infer](@ref mim::Infer)s_ in MimIR) that may pop up due to type inference.
Matching built-ins, i.e. all subclasses of [Def](@ref mim::Def), works differently than matching [Axiom](@ref mim::Axiom)s.

### Upcast for Built-ins {#cast_builtin}

Methods beginning with

- `isa` work like a `dynamic_cast` with a runtime check and return `nullptr` if the cast is not possible, while
- those beginning with `as` are more like a `static_cast` and `assert` via its `isa` sibling in the `Debug` build that the cast is correct.

#### Upcast

`Def::isa`/`Def::as` allows for an _upcast_ that matches both _mutables_ and _immutables_:

```cpp
void foo(Ref def) {
    if (auto sigma = def->isa<Sigma>()) {
        // sigma is of type "const Sigma*" and could be a mutable or an immutable
    }

    // sigma of type "const Sigma*" and could be an immutable or a mutable
    // asserts, if def is not a Sigma
    auto sigma = def->as_imm<Sigma>();
}
```

#### Upcast for Immutables

[Def::isa_imm](@ref mim::Def::isa_imm)/[Def::as_imm](@ref mim::Def::as_imm) allows for an *upcast* and **only** matches _immutables_:

```cpp
void foo(Ref def) {
    if (auto imm = def->isa_imm()) {
        // imm of type "const Def*" and is an immutable
    }

    if (auto sigma = def->isa_imm<Sigma>()) {
        // sigma is of type "const Sigma*" and is an immutable Sigma
    }

    // sigma of type "const Sigma*" and *must* be an immutable Sigma - otherwise, asserts
    auto sigma = def->as_imm<Sigma>();
}
```

#### Upcast for Mutables

[Def::isa_mut](@ref mim::Def::isa_mut)/[Def::as_mut](@ref mim::Def::as_mut) allows for an *upcast* and **only** matches _mutables_.
By doing so, it removes the `const` qualifier and gives you access to the **non**-`const` methods that only make sense for _mutables_:

```cpp
void foo(Ref def) {
    if (auto mut = def->isa_mut()) {
        // mut of type "Def*" - "const" has been removed!
        // This gives you access to the non-const methods:
        auto var = mut->var();
        auto stub = mut->stub(world, type, debug)
        // ...
    }

    if (auto lam = def->isa_mut<Lam>()) {
        // lam of type "Lam*" - "const" has been removed!
    }

    // lam of type "Lam*" and *must* be a mutable Lam.
    // Otherwise, an assert will fire.
    auto lam = def->as<Lam>();
}
```

Checking via `Def::isa`/`Def::as` a `Def*` has the same effect as using [Def::isa_mut](@ref mim::Def::isa_mut)/[Def::isa_mut](@ref mim::Def::as_mut) since the scrutinee must be already a *mutable* due to the lack of the `const` qualifier:

```cpp
void foo(Def* def) { // note the lack of "const" here
    if (auto sigma = def->isa<Sigma>()) {
        // sigma is of type Sigma* and is a mutable
    }
    if (auto sigma = def->isa_mut<Sigma>()) {
        // sigma is of type Sigma* and is a mutable
    }

    // sigma of type "Sigma*" and is a mutable - otherwise, asserts
    auto sigma = def->as<Sigma>();
}
```

#### Matching Literals {#cast_lit}

Often, you want to match a [Lit](@ref mim::Lit)eral and grab its content.
You can use [Lit::isa](@ref mim::Lit::isa)/[Lit::as](@ref mim::Lit::as) for this:

```cpp
void foo(Ref def) {
    if (auto lit = Lit::isa(def)) {
        // lit is of type "std::optional<u64>"
        // It's your responsibility that the grabbed value makes sense as u64.
    }
    if (auto lit = Lit::isa<f32>(def)) {
        // lit is of type "std::optional<f32>"
        // It's your responsibility that the grabbed value makes sense as f32.
    }

    // asserts if def is not a Lit.
    auto lu64 = Lit::as(def);
    auto lf32 = Lit::as<f32>(def);
}
```

#### Summary

The following table summarizes all important casts:

| `dynamic_cast` <br> `static_cast`               | Returns                                                                                                                         | If `def` is a ...                  |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| `def->isa<Lam>()` <br> `def->as<Lam>()`         | `const Lam*`                                                                                                                    | [Lam](@ref mim::Lam)               |
| `def->isa_imm<Lam>()` <br> `def->as_imm<Lam>()` | `const Lam*`                                                                                                                    | **immutable** [Lam](@ref mim::Lam) |
| `def->isa_mut<Lam>()` <br> `def->as_mut<Lam>()` | `Lam*`                                                                                                                          | **mutable** [Lam](@ref mim::Lam)   |
| `Lit::isa(def)` <br> `Lit::as(def)`             | [std::optional](https://en.cppreference.com/w/cpp/utility/optional)`<`[nat_t](@ref mim::nat_t)`>` <br> [nat_t](@ref mim::nat_t) | [Lit](@ref mim::Lit)               |
| `Lit::isa<f32>(def)` <br> `Lit::as<f32>(def)`   | [std::optional](https://en.cppreference.com/w/cpp/utility/optional)`<`[f32](@ref mim::f32)`>` <br> [f32](@ref mim::f32)         | [Lit](@ref mim::Lit)               |

#### Further Casts

There are also some additional checks available that usually come as `static` methods and either return a pointer or `Ref` to the checked entity or `nullptr`.
Here are some examples:

```cpp
void foo(Ref def) {
    if (auto size = Idx::isa(def)) {
        // def = Idx size
    }

    if (auto lam = Lam::isa_mut_cn(def)) {
        // def isa mutable Lam of type Cn T
    }

    if (auto pi = Pi::isa_basicblock(def)) {
        // def is a Pi whose co-domain is bottom and which is not returning
    }
}
```

### Matching Axioms {#cast_axiom}

You can match [Axiom](@ref mim::Axiom)s via

- mim::match which is again similar to a `dynamic_cast` with a runtime check and returns [a wrapped](@ref mim::Match::Match) `nullptr` (see below), if the cast is not possible, or
- mim::force which is again more like a `static_cast` and `assert`s via its mim::match sibling in the `Debug` build that the cast is correct.

This will yield a [Match](@ref mim::Match)`<Id, D>` which just wraps a `const D*`.
`Id` is the `enum` of the corresponding `tag` of the [matched Axiom](@ref anatomy).
Usually, `D` will be an [App](@ref mim::App) because most [Axiom](@ref mim::Axiom)s inhabit a [function type](@ref mim::Pi).
Otherwise, it may wrap a [Def](@ref mim::Def) or other subclasses of it.
For instance, [match](@ref mim::match)ing `%%mem.M` yields [Match](@ref mim::Match)`<`[mem::M](@ref mim::plug::mem::M), [Def](@ref mim::Def)`>`.

By default, MimIR assumes that the magic of an [Axiom](@ref mim::Axiom) happens when applying the final argument to a curried [Axiom](@ref mim::Axiom).
For example, [match](@ref mim::match)ing a `%%mem.load` will only trigger for the final [App](@ref mim::App) of the curried call

```
%mem.load (T, as) (mem, ptr)
```

while

```
%mem.load (T, as)
```

will **not** match.
The wrapped [App](@ref mim::App) inside the [Match](@ref mim::Match) refers to the last [App](@ref mim::App) of the curried call.
So in this example

- mim::App::arg() is `(mem, ptr)` and
- mim::App::callee() is `%%mem.load (T, as)`.

  Use mim::App::decurry() to directly get the mim::App::callee() as mim::App.

If you want to design an [Axiom](@ref mim::Axiom) that returns a function, you can [fine-adjust the trigger point](@ref normalization) of a mim::match / mim::force.

#### w/o Subtags

In order to match an [Axiom](@ref mim::Axiom) **without** any subtags like `%%mem.load`, do this:

```cpp
void foo(Ref def) {
    if (auto load = match<mem::load>(def)) {
        auto [mem, ptr]            = load->args<2>();
        auto [pointee, addr_space] = load->decurry()->args<2>();
    }

    // def must match as a mem::load - otherwise, asserts
    auto load = force<mem::load>(def);
}
```

#### w/ Subtags

In order to match an [Axiom](@ref mim::Axiom) **with** subtags like `%%core.wrap`, do this:

```cpp
void foo(Ref def) {
    if (auto wrap = match<core::wrap>(def)) {
        auto [a, b] = wrap->args<2>();
        auto mode   = wrap->decurry()->arg();
        switch (wrap.id()) {
            case core::wrap::add: // ...
            case core::wrap::sub: // ...
            case core::wrap::mul: // ...
            case core::wrap::shl: // ...
        }
    }

    if (auto add = match(core::wrap::add, def)) {
        auto [a, b] = add->args<2>();
        auto mode   = add->decurry()->arg();
    }

    // def must match as a core::wrap - otherwise, asserts
    auto wrap = force<core::wrap>(def);

    // def must match as a core::wrap::add - otherwise, asserts
    auto add = force(core::wrap::add, def);
}
```

#### Summary

The following table summarizes all important casts:

| `dynamic_cast` <br> `static_cast`                                | Returns                                                                                    | If `def` is a ...               |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------- |
| `match<mem::load>(def)` <br> `force<mem::load>(def)`             | [Match](@ref mim::Match)`<`[mem::load](@ref mim::plug::mem.load), [App](@ref mim::App)`>`  | `%%mem.load (T, as) (mem, ptr)` |
| `match<core::wrap>(def)` <br> `force<core::wrap>(def)`           | [Match](@ref mim::Match)`<`[core::wrap](@ref mim::plug::mem.load), [App](@ref mim::App)`>` | `%%core.wrap.??? s m (a, b)`    |
| `match(core::wrap::add, def)` <br> `force(core::wrap::add, def)` | [Match](@ref mim::Match)`<`[core::wrap](@ref mim::plug::mem.load), [App](@ref mim::App)`>` | `%%core.wrap.add s m (a, b)`    |

## Working with Indices

There are essentially **three** ways of retrieving the number of elements of something in MimIR.

### Arity

This is the number of elements of to [extract](@ref mim::Extract)/[insert](@ref mim::Insert) a single element.
Note that the number of elements may be unknown at compile time such as in `‹n; 0›`.

### Proj

mim::Def::num_projs is the same as mim::Def::arity, if the arity is a mim::Lit.
Otherwise, it is simply `1`.
This concept only exists in the C++-API to give the programmer the illusion to work with n-ary functions, e.g.:

```cpp
for (auto dom : pi->doms()) { /*...*/ }
for (auto var : lam->vars()) { /*...*/ }
```

But in reality, all functions have exactly one domain and one codomain.

#### Thresholded Variants

In additition, there are thresholded variants available that are prefixed with a `t` and take mim::Flags::scalarize_threshold (`--scalarize-threshold`) into account.
The method mim::Def::num_tprojs returns the same as mim::Def::num_projs, but will simply yield `1`, if the arity exceeds the threshold.
mim::Def::tproj, mim::Def::tprojs, mim::Lam::tvars, etc. work accordingly.

**See also:**

- @ref proj "Def::proj"
- @ref var "Def::var"
- @ref pi_dom "Pi::dom"
- @ref pi_codom "Pi::codom"
- @ref lam_dom "Lam::dom"
- @ref lam_codom "Lam::codom"
- @ref app_arg "App::arg"

### Shape

TODO

### Summary

| Expression            | Class                    | [artiy](@ref mim::Def::arity) | [isa_lit_artiy](@ref mim::Def::isa_lit_arity) | [as_lit_artiy](@ref mim::Def::as_lit_arity) | [num_projs](@ref mim::Def::num_projs) | [num_tprojs](@ref mim::Def::num_tprojs) |
| --------------------- | ------------------------ | ----------------------------- | --------------------------------------------- | ------------------------------------------- | ------------------------------------- | --------------------------------------- |
| `(0, 1, 2)`           | [Tuple](@ref mim::Tuple) | `3`                           | `3`                                           | `3`                                         | `3`                                   | `3`                                     |
| `‹3; 0›`              | [Pack](@ref mim::Pack)   | `3`                           | `3`                                           | `3`                                         | `3`                                   | `3`                                     |
| `‹n; 0›`              | [Pack](@ref mim::Pack)   | `n`                           | `std::nullopt`                                | asserts                                     | `1`                                   | `1`                                     |
| `[Nat, Bool, Nat]` | [Sigma](@ref mim::Sigma) | `3`                           | `3`                                           | `3`                                         | `3`                                   | `3`                                     |
| `«3; Nat»`           | [Arr](@ref mim::Arr)     | `3`                           | `3`                                           | `3`                                         | `3`                                   | `3`                                     |
| `«n; Nat»`           | [Arr](@ref mim::Arr)     | `n`                           | `std::nullopt`                                | asserts                                     | `1`                                   | `1`                                     |
| `x: [Nat, Bool]`    | [Var](@ref mim::Var)     | `2`                           | `2`                                           | `2`                                         | `2`                                   | `2`                                     |
| `‹32; 0›`             | [Pack](@ref mim::Pack)   | `32`                          | `32`                                          | `32`                                        | `32`                                  | `1`                                     |

The last line assumes mim::Flags::scalarize_threshold = 32.

## Iterating over the Program

There are several ways of doing this.
It depends on what exactly you want to achieve and how much structure you need during the traversal.
The simplest way is to kick off with [World::externals](@ref mim::World::externals) and recursively run over [Def::extended_ops](@ref mim::Def::extended_ops) like this:

```cpp
    DefSet done;
    for (const auto& [_, mut] : worldexternals())
        visit(done, mut);

    void visit(DefSet& done, const Def* def) {
        if (!done.emplace(def).second) return;

        do_sth(def);

        for (auto op : def->extended_ops())
            visit(done, op);
    }

```

However, you will most likely want to use the [pass](passes.md) or the phase infrastructure.
