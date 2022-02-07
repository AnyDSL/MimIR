# Developer Guide

[TOC]

This guide summaries typicical idioms you want to use when working with Thorin as a developer.

## Defs and the World

[Def](@ref thorin::Def) is the base class for @em all nodes/expressions in Thorin.
TODO

### Hash Consing

[Hash consing](https://en.wikipedia.org/wiki/Hash_consing) TODO

## Structural vs. Nominal Defs

| Structural                                                            | Nominal                       |
|-----------------------------------------------------------------------|-------------------------------|
| `const`                                                               | @em not `const`               |
| immutable                                                             | mutable                       |
| ops form [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph)  | ops may be cyclic             |
| no recursion                                                          | may be recursive              |
| no [Var](@ref thorin::Var)                                            | has [Var](@ref thorin::Var)   |
| hash consed                                                           | each new instance is fresh    |

```{.cpp}
void foo(const Def* def) {
    if (auto nom = def->isa_nom()) {
        // nom of type Def* - const has been removed!
        auto var = nom->var();
    }

    if (auto lam = def->isa_nom<Lam>()) {
        // lam of type Lam* - const has been removed!
    }
}
```

## Iterating over the Program

There are several ways of doing this.
It depends on what exactly you want to achieve and how much structure you need during the traversal.
The simplest way is to kick off with [World::externals](@ref thorin::World::externals) and recursively run over [Def::extended_ops](@ref thorin::Def::extended_ops) like this:
```{.cpp}
    DefSet done;
    for (const auto& [_, nom] : world.externals()) visit(done, nom);

    void visit(DefSet& done, const Def* def) {
        if (!done.emplace(def).second) return;

        do_sth(def);

        for (auto op : def->extended_ops())
            visit(done, op);
    }

```

[World::visit](@ref thorin::World::visit) allows you to visit @em all top-level scopes with
```{.cpp}
    world.visit([&](const Scope& scope) {
        // ...
    });
```

Finally, if you really want to transform the program you probably want to use the [pass](passes.md) infrastructure.
