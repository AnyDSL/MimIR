# Developer Guide {#dev}

[TOC]

This guide summaries typical idioms you want to use when working with Thorin as a developer.

## Basics

Let's jump straight into an example.
\include "examples/hello.cpp"

[Driver](@ref thorin::Driver) is the first class that you want to allocate.
It keeps track of a few global variables like some [flags](@ref thorin::Flags) or the [log](@ref thorin::Log).
Here, the log is set up to output to `std::cerr` with thorin::Log::Level::Debug (see also @ref clidebug).

Then, we load the plugins [compile](@ref compile) and [core](@ref core), which in turn will load the plugin [mem](@ref mem).
A plugin consists of a shared object (`.so`/`.dll`) and a `.thorin` file.
The shared object contains [Passes](@ref thorin::Pass), [normalizers](@ref thorin::Axiom::normalizer), and so on.
The `.thorin` file contains the [axiom](@ref thorin::Axiom) declarations and links the normalizers with the [axioms](@ref thorin::Axiom).
For this reason, we need to allocate the thorin::fe::Parser to parse the `.thorin` file; thorin::fe::Parser::plugin will also load the shared object.
The [driver](@ref thorin::Driver) keeps track of all plugins.

Next, we create actual code.
[Def](@ref thorin::Def) is the base class for **all** nodes/expressions in Thorin.
Each [Def](@ref thorin::Def) is a node in a graph which is maintained in the [World](@ref thorin::World).
The [World](@ref thorin::World) is essentially a big hash set where all [Defs](@ref thorin::Def) are tossed into.
The [World](@ref thorin::World) provides factory methods to create all kind of different [Defs](@ref thorin::Def).
Here, we create the `main` function.
In direct style, its type looks like this:
```
[%mem.M, I32, %mem.Ptr (I32, 0)] -> [%mem.M, I32]]
```
Converted to [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style) this type looks like this:
```
.Cn [%mem.M, I32, %mem.Ptr (I32, 0), .Cn [%mem.M, I32]]
```
The `%%mem.M` type is a type that keeps track of side effects that may occur.
Since, `main` introduces [variables](@ref thorin::Var) we must create a **[mutable](@ref mut)** [Lam](@ref thorin::Lam)bda.
The only thing `main` is doing, is to invoke its `ret`urn continuation with `mem` and `argc` as argument:
```
ret (mem, argc)
```
It is also important to make `main` [external](@ref thorin::Def::make_external).
Otherwise, Thorin will simply remove this function.

We optimize the program, emit an [LLVM assembly file](https://llvm.org/docs/LangRef.html), and compile it via `clang`.
Finally, we execute the generated program with `./hello a b c` and [output](@ref fmt) its exit code - which should be `4`.

## Immutables vs. Mutables {#mut}

There are two different kind of [Defs](@ref thorin::Def) in Thorin: *mutables* and *immutables*:

| **Immutable**                                                         | **Mutable**                                                                   |
|-----------------------------------------------------------------------|-------------------------------------------------------------------------------|
| *must be* `const`                                                     | *may be* **non**-`const`                                                      |
| ops form [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph)  | ops may be cyclic                                                             |
| no recursion                                                          | may be recursive                                                              |
| no [Var](@ref thorin::Var)                                            | has [Var](@ref thorin::Var); get with [Def::var](@ref thorin::Def::var)       |
| build ops first, then the actual node                                 | build the actual node first, then [set](@ref thorin::Def::set) the ops        |
| [Hash consed](https://en.wikipedia.org/wiki/Hash_consing)             | each new instance is fresh                                                    |
| [Def::rebuild](@ref thorin::Def::rebuild)                             | [Def::stub](@ref thorin::Def::stub)                                           |

## Matching IR

Thorin provides different means to scrutinize [Defs](@ref thorin::Def).
Usually, you will encounter a [Def](@ref thorin::Def) as [Ref](@ref thorin::Ref) which is just a wrapper for a `const Def*`.
Its purpose is to resolve varialbes (called *[Infer](@ref thorin::Infer)s* in Thorin) that may pop up due to type inference.
Matching built-ins, i.e. all subclasses of [Def](@ref thorin::Def), works differently than matching [Axiom](@ref thorin::Axiom)s.

### Upcast for Built-ins {#cast_builtin}

Methods beginning with
* `isa` work like a `dynamic_cast` with a runtime check and return `nullptr` if the cast is not possible, while
* those beginning with `as` are more like a `static_cast` and `assert` via its `isa` sibling in the `Debug` build that the cast is correct.

#### Upcast

[Def::isa](@ref thorin::RuntimeCast::isa)/[Def::as](@ref thorin::RuntimeCast::as) allows for an *upcast* that matches both *mutables* and *immutables*:
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

[Def::isa_imm](@ref thorin::Def::isa_imm)/[Def::as_imm](@ref thorin::Def::as_imm) allows for an *upcast* and **only** matches *immutables*:
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

[Def::isa_mut](@ref thorin::Def::isa_mut)/[Def::as_mut](@ref thorin::Def::as_mut) allows for an *upcast* and **only** matches *mutables*:
```cpp
void foo(Ref def) {
    if (auto mut = def->isa_mut()) {
        // mut of type "Def*" - "const" has been removed!
        // This gives you access to the non-const methods that only make sense for mutables:
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
Checking via [Def::isa](@ref thorin::RuntimeCast::isa)/[Def::as](@ref thorin::RuntimeCast::as) a `Def*` has the same effect as using [Def::isa_mut](@ref thorin::Def::isa_mut)/[Def::isa_mut](@ref thorin::Def::as_mut) since the scrutinee must be already a *mutable* due to the lack of the `const` qualifier:
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

Often, you want to match a [Lit](@ref thorin::Lit)eral and grab its content.
You can use thorin::isa_lit / thorin::as_lit for this:
```cpp
void foo(Ref def) {
    if (auto lit = isa_lit(def)) {
        // lit is of type "std::optional<u64>"
        // It's your repsonsibility that the grabbed value makes sense as u64.
    }
    if (auto lit = isa_lit<f32>(def)) {
        // lit is of type "std::optional<f32>"
        // It's your repsonsibility that the grabbed value makes sense as f32.
    }

    // asserts if def is not a Lit.
    auto lu64 = as_lit(def);
    auto lf32 = as_lit<f32>(def);
}
```

#### Summary

The following table summarizes all important casts:

| `dynamic_cast`        <br> `static_cast`        | Returns                                                                                                                               | If `def` is a ...                     |
|-------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------|
| `def->isa<Lam>()`     <br> `def->as<Lam>()`     | `const Lam*`                                                                                                                          | [Lam](@ref thorin::Lam)               |
| `def->isa_imm<Lam>()` <br> `def->as_imm<Lam>()` | `const Lam*`                                                                                                                          | **immutable** [Lam](@ref thorin::Lam) |
| `def->isa_mut<Lam>()` <br> `def->as_mut<Lam>()` | `Lam*`                                                                                                                                | **mutable** [Lam](@ref thorin::Lam)   |
| `isa_lit(def)`        <br> `as_lit(def)`        | [std::optional](https://en.cppreference.com/w/cpp/utility/optional)`<`[nat_t](@ref thorin::nat_t)`>` <br> [nat_t](@ref thorin::nat_t) | [Lit](@ref thorin::Lit)               |
| `isa_lit<f32>(def)`   <br> `as_lit<f32>(def)`   | [std::optional](https://en.cppreference.com/w/cpp/utility/optional)`<`[f32](@ref thorin::f32)`>`     <br> [f32](@ref thorin::f32)     | [Lit](@ref thorin::Lit)               |

### Matching Axioms {#cast_axiom}

You can match [Axiom](@ref thorin::Axiom)s via
* thorin::match which is again similar to a `dynamic_cast` with a runtime check and returns [a wrapped](@ref thorin::Match::Match) `nullptr` (see below), if the cast is not possible, or
* thorin::force which is again more like a `static_cast` and `assert`s via its thorin::match sibling in the `Debug` build that the cast is correct.

This will yield a [Match](@ref thorin::Match)`<Id, D>` which just wraps a `const D*`.
`Id` is the `enum` of the corresponding `tag` of the [matched Axiom](@ref anatomy).
Usually, `D` will be an [App](@ref thorin::App) because most [Axiom](@ref thorin::Axiom)s inhabit a [function type](@ref thorin::Pi).
Otherwise, it may wrap a [Def](@ref thorin::Def) or other subclasses of it.
For instance, [match](@ref thorin::match)ing `%%mem.M` yields [Match](@ref thorin::Match)`<`[mem::M](@ref thorin::mem::M), [Def](@ref thorin::Def)`>`.

By default, Thorin assumes that the magic of an [Axiom](@ref thorin::Axiom) happens when applying the final argument to a curried [Axiom](@ref thorin::Axiom).
If you want to design an [Axiom](@ref thorin::Axiom) that really returns a function, you can [fine-adjust the trigger point](@ref normalization) of a thorin::match / thorin::force.

#### w/o Subtags

In order to match an [Axiom](@ref thorin::Axiom) **without** any subtags like `%%mem.load`, do this:
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
The `match` only triggers for the final thorin::App of the curried call `%%mem.load (T, as) (mem, ptr)` and `%%mem.load (T, as)` will **not** match as explained above.
This will yield a [Match](@ref thorin::Match) which *usually* just wraps a `const` [App](@ref thorin::App)`*` but may wrap a [Def](@ref thorin::Def) or other subclasses of it, if the type of the Axiom is **not** a [function type](@ref thorin::Pi).

#### w/ Subtags

In order to match an [Axiom](@ref thorin::Axiom) **with** subtags like `%%core.wrap`, do this:
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

| `dynamic_cast`                <br> `static_cast`                 | Returns                                                                                       | If `def` is a ...               |
|------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|---------------------------------|
| `match<mem::load>(def)`       <br> `force<mem::load>(def)`       | [Match](@ref thorin::Match)`<`[mem::load](@ref thorin::mem.load), [App](@ref thorin::App)`>`  | `%%mem.load (T, as) (mem, ptr)` |
| `match<core::wrap>(def)`      <br> `force<core::wrap>(def)`      | [Match](@ref thorin::Match)`<`[core::wrap](@ref thorin::mem.load), [App](@ref thorin::App)`>` | `%%core.wrap.??? s m (a, b)`    |
| `match(core::wrap::add, def)` <br> `force(core::wrap::add, def)` | [Match](@ref thorin::Match)`<`[core::wrap)](ref thorin::mem.load), [App](@ref thorin::App)`>` | `%%core.wrap.add s m (a, b)`    |

## Iterating over the Program

There are several ways of doing this.
It depends on what exactly you want to achieve and how much structure you need during the traversal.
The simplest way is to kick off with [World::externals](@ref thorin::World::externals) and recursively run over [Def::extended_ops](@ref thorin::Def::extended_ops) like this:
```cpp
    DefSet done;
    for (const auto& [_, mut] : world.externals())
        visit(done, mut);

    void visit(DefSet& done, const Def* def) {
        if (!done.emplace(def).second) return;

        do_sth(def);

        for (auto op : def->extended_ops())
            visit(done, op);
    }

```
However, you will most likely want to use the [pass](passes.md) or the phase infrastructure.
