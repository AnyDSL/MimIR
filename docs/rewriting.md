# Rewriting {#rewriting}

[TOC]

In MimIR, rewriting means rebuilding an IR graph, usually into a target [`World`](@ref mim::World), while preserving sharing and handling mutable nodes correctly.
The rewriting infrastructure is used for tasks like copying IR between worlds, rebuilding terms after substitutions, and reconstructing terms after local changes.

## Rewriter {#rewriter}

[`Rewriter`](@ref mim::Rewriter) is the general mechanism for recursively rebuilding IR.

Conceptually, it walks a graph of [`Def`](@ref mim::Def)s and constructs corresponding nodes in a target world.
While doing so, it maintains a mapping from old nodes to new nodes.
This is what makes rewriting graph-aware rather than tree-only:
if the same old node is referenced multiple times, the rewritten graph will reuse the same rebuilt node instead of duplicating it.

A useful way to think about [`Rewriter`](@ref mim::Rewriter) is:

> [`Rewriter`](@ref mim::Rewriter) rebuilds a [`Def`](@ref mim::Def) graph into a target [`World`](@ref mim::World) while preserving sharing and handling recursive mutables safely.

The mapping is also the basis for termination on cyclic or recursive structures.
For mutable nodes, rewriting typically proceeds by first creating a stub, recording the mapping from old mutable to new mutable, and only then rewriting and filling its operands.
This allows recursive references to resolve to the stub instead of recursing forever.

So the two key ideas behind [`Rewriter`](@ref mim::Rewriter) are:

- memoization of already rewritten nodes
- stub-based reconstruction of mutable or recursive nodes

At a high level, [`Rewriter`](@ref mim::Rewriter) preserves:

- graph sharing
- debug metadata
- recursive structure through mutable stubs

### Scoping

[`Rewriter`](@ref mim::Rewriter) uses a stack of mappings rather than a single global map.
This allows temporary, scoped rewrites.
This matters whenever rewriting enters a context where some old node should mean something different locally than globally.
Inner mappings shadow outer ones, and leaving the scope removes the temporary bindings again.
This scoped structure is important for binder-sensitive rewrites and for transformations that temporarily substitute certain nodes while rebuilding a larger term.

### Typical Usage

Rebuild a term into another world:

```cpp
auto rw      = mim::Rewriter(new_world);
auto new_def = rw.rewrite(old_def);
```

## VarRewriter {#varrewriter}

[`VarRewriter`](@ref mim::VarRewriter) is a specialized form of [`Rewriter`](@ref mim::Rewriter) for variable substitution.
It extends the generic rewriting machinery with knowledge about [`Var`](@ref mim::Var)s and binders.
Instead of rebuilding everything indiscriminately, it only rewrites parts of the graph that may actually mention substituted variables.

A good summary is:

> [`VarRewriter`](@ref mim::VarRewriter) performs scoped variable substitution on top of the general rewriting framework.

The main extra idea is that [`VarRewriter`](@ref mim::VarRewriter) tracks which variables are currently relevant for substitution and uses free-variable information to decide whether a node needs to be rebuilt at all.
If a subgraph cannot contain any of the substituted variables, it can be reused unchanged.
This makes substitution much more efficient on large terms: unaffected subgraphs are skipped entirely.

### Scoped Substitution

Like [`Rewriter`](@ref mim::Rewriter), [`VarRewriter`](@ref mim::VarRewriter) is scope-aware.
This is essential because substitution interacts with binders.
When rewriting enters a mutable binder that introduces a variable, [`VarRewriter`](@ref mim::VarRewriter) tracks that variable in the current scope.
This allows it to reason correctly about which free variables are relevant in nested terms.

So [`VarRewriter`](@ref mim::VarRewriter) is not just “replace [`Var`](@ref mim::Var) with [`Def`](@ref mim::Def)”.
It is substitution that respects binder structure and avoids rebuilding irrelevant parts of the graph.

### Typical Usage

Substitute one variable with one argument:

```cpp
auto rw     = mim::VarRewriter(var, arg);
auto result = rw.rewrite(body);
```

Build it incrementally:

```cpp
auto rw = mim::VarRewriter(world);
rw.add(var1, arg1)
  .add(var2, arg2);

auto result = rw.rewrite(def);
```

Conceptually, this rewrites only those parts of `def` whose free variables intersect the substituted variables.

## Relationship between both

[`VarRewriter`](@ref mim::VarRewriter) is a specialization of [`Rewriter`](@ref mim::Rewriter):

- [`Rewriter`](@ref mim::Rewriter) provides the general graph-rebuilding machinery:
  - rebuild nodes into a target world
  - preserve sharing
  - handle mutable recursion through stubs
  - support scoped mappings

  If you just want to rebuild IR, use [`Rewriter`](@ref mim::Rewriter):
  > What is the corresponding version of this IR in another rewriting context?

- [`VarRewriter`](@ref mim::VarRewriter) adds substitution-specific behavior:
  - map variables to replacement terms
  - track relevant variables by scope
  - skip unaffected subgraphs using free-variable information

  If you want to substitute variables while rebuilding, use [`VarRewriter`](@ref mim::VarRewriter):
  > What do I get if I substitute these variables and rebuild only where necessary?
