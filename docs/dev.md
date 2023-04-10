# Developer Guide {#dev}

[TOC]

This guide summaries typicical idioms you want to use when working with Thorin as a developer.

## Compiling and Executing

Here is a small example that first constructs a `main` function and simply returns the `argc`:
```cpp
    Driver driver;
    World& w    = driver.world();
    auto parser = fe::Parser(w);
    for (auto plugin : {"compile", "mem", "core", "math"}) parser.plugin(plugin);

    auto mem_t  = mem::type_mem(w);
    auto i32_t  = w.type_int(32);
    auto argv_t = mem::type_ptr(mem::type_ptr(i32_t));

    // Cn [mem, i32, Cn [mem, i32]]
    auto main_t = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main = w.mut_lam(main_t)->set("main");
    auto [mem, argc, argv, ret] = main->vars<4>();
    main->app(ret, {mem, argc});
    main->make_external();

    PipelineBuilder builder;
    optimize(w, builder);

    std::ofstream file("test.ll");
    Stream s(file);
    backends["ll"](w, std::cout);
    file.close();

    std::system("clang test.ll -o test");
    assert(4 == WEXITSTATUS(std::system("./test a b c")));
```
TODO explain

## Defs and the World

[Def](@ref thorin::Def) is the base class for @em all nodes/expressions in Thorin.
TODO

### Hash Consing

Each `Def` is a node in a graph which is maintained in the [World](@ref thorin::World).
[Hash consing](https://en.wikipedia.org/wiki/Hash_consing) TODO

## Immutables vs. Mutables

| **Immutable**                                                         | **Mutable**                                                                   |
|-----------------------------------------------------------------------|-------------------------------------------------------------------------------|
| immutable                                                             | mutable                                                                       |
| *must be* `const`                                                     | *may be* **non**-`const`                                                      |
| ops form [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph)  | ops may be cyclic                                                             |
| no recursion                                                          | may be recursive                                                              |
| no [Var](@ref thorin::Var)                                            | has [Var](@ref thorin::Var); get with [Def::var](@ref thorin::Def::var)       |
| hash consed                                                           | each new instance is fresh                                                    |
| build ops first, then the actual node                                 | build the actual node first, then [set](@ref thorin::Def::set) the ops        |
| [Def::rebuild](@ref thorin::Def::rebuild)                             | [Def::stub](@ref thorin::Def::stub)                                           |

## Matching IR

### Matching Builtins

Usually, you will encounter a [Def](@ref thorin::Def) as [Ref](@ref thorin::Ref) which is just a wrapper for a `const Def*`.
Use [Def::isa_mut](@ref thorin::Def::isa_mut) to check whether a specific [Ref](@ref thorin::Ref)/`const Def*` is in fact mutable to cast away the `const` or [Def::as_mut](@ref thorin::Def::as_mut) to force this cast (and assert if not possible):
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
You can also check whether a specific [Ref](@ref thorin::Ref)/`const Def*` is in fact an *immutable*:
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
If you just check via [Def::isa](@ref thorin::RuntimeCast::isa)/[Def::as](@ref thorin::RuntimeCast::as) a [Ref](@ref thorin::Ref)/`const Def*`, you match either a *mutable* or an *immutable*.
```cpp
void foo(Ref def) {
    if (auto sigma = def->isa<Sigma>()) {
        // sigma is of type const Sigma* and could be a mutable or an immutable
    }

    // sigma of type "const Sigma*" and could be an immutable or a mutable
    // asserts, if def is not a Sigma
    auto sigma = def->as_imm<Sigma>();
}
```
Checking via [Def::isa](@ref thorin::RuntimeCast::isa)/[Def::as](@ref thorin::RuntimeCast::as) a `Def*` has the same effect as using [Def::isa_mut](@ref thorin::Def::isa_mut)/[Def::isa_mut](@ref thorin::Def::as_mut) since the scrutinee must be a *mutable* due to the lack of the `const` qualifier:
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

### Matching Axioms

You can match [axioms](@ref thorin::Axiom) via thorin::match / thorin::force.
By default, Thorin assumes that the magic of an [axiom](@ref thorin::Axiom) happens when applying the final argument to a curried [axiom](@ref thorin::Axiom).
If you want to design an [axiom](@ref thorin::Axiom) that really returns a function, you can [fine-adjust the trigger point](@ref thorin::Axiom::normalizer) of a thorin::match / thorin::force.

In order to match an [axiom](@ref thorin::Axiom) **without** any subtags like `%%mem.load`, do this:
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

In order to match an [axiom](@ref thorin::Axiom) **with** subtags like `%%core.wrap`, do this:
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
