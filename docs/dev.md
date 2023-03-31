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
| must be `const`                                                       | maybe non-`const`                                                             |
| immutable                                                             | mutable                                                                       |
| ops form [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph)  | ops may be cyclic                                                             |
| no recursion                                                          | may be recursive                                                              |
| no [Var](@ref thorin::Var)                                            | has [Var](@ref thorin::Var); get with [Def::var](@ref thorin::Def::var)       |
| hash consed                                                           | each new instance is fresh                                                    |
| build ops first, then the actual node                                 | build the actual node first, then [set](@ref thorin::Def::set) the ops        |
| [Def::rebuild](@ref thorin::Def::rebuild)                             | [Def::stub](@ref thorin::Def::stub)                                           |

Usually, you will encounter `defs` as `const Def*`.
Use [Def::isa_mut](@ref thorin::Def::isa_mut) to check whether a specific `Def` is in fact mutable to cast away the `const` or [Def::as_mut](@ref thorin::Def::as_mut) to force this cast (and assert if not possible):
```cpp
void foo(const Def* def) {
    if (auto mut = def->isa_mut()) {
        // mut of type Def* - const has been removed!
        // This gives give you access to the non-const methods that only make sense for mutable:
        auto var = mut->var();
        auto stub = mut->stub(world, type, debug)
        // ...
    }

    // ...

    if (auto lam = def->isa_mut<Lam>()) {
        // lam of type Lam* - const has been removed!
    }
}
```
You can also check whether a specific `def` is really an *imm*utable:
```cpp
void foo(const Def* def) {
    if (auto s = def->isa_imm()) {
        // s cannot be a mutable
    }

    // ...

    if (auto sigma = def->isa_imm<Sigma>()) {
        // sigma is of type const Sigma* and cannot be a mutable Sigma
    }
}
```

## Matching IR

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
