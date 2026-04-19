# Phases {#phases}

[TOC]

At a high level, a phase is a compiler transformation or analysis step that runs in isolation.
Unlike older pass-style pipelines, phases are meant to do one thing at a time and compose in a straightforward sequence.
See also the [Rewriting Guide](@ref rewriting), since several phase families are built directly on top of [`Rewriter`](@ref mim::Rewriter).

## Overview

A [`Phase`](@ref mim::Phase) is a [`Stage`](@ref mim::Stage) with a single entry point, [`run()`](@ref mim::Phase::run), which wraps the actual implementation in [`start()`](@ref mim::Phase::start).

Conceptually, a phase is one of three things:

- a pure analysis over the current [`World`](@ref mim::World)
- a rewrite from the current [`World`](@ref mim::World) into a new one
- a traversal over selected reachable mutables

The framework provides dedicated base classes for these common cases.

## Phase {#phases_phase}

[`Phase`](@ref mim::Phase) is the minimal base class.

A phase has:

- a name or annex,
- access to the current [`World`](@ref mim::World),
- a [`run()`](@ref mim::Phase::run) wrapper for logging and verification,
- a [`todo()`](@ref mim::Phase::todo) accessor backed by the internal `todo_` flag for fixed-point iteration.

The important design point is the internal `todo_` flag exposed through [`todo()`](@ref mim::Phase::todo):

> A phase sets `todo_ = true` if its work discovered that another round is needed.

This is used by [`PhaseMan`](@ref mim::PhaseMan) to drive fixed-point pipelines.

### Typical Shape

A custom phase usually derives from [`Phase`](@ref mim::Phase) and implements [`start()`](@ref mim::Phase::start):

```cpp
class MyPhase : public mim::Phase {
public:
    MyPhase(World& world)
        : Phase(world, "my_phase") {}

private:
    void start() override {
        // do work here
    }
};
```

Run it with:

```cpp
mim::Phase::run<MyPhase>(world);
```

## Analysis {#phases_analysis}

[`Analysis`](@ref mim::Analysis) is the base class for phases that **inspect** the current world while reusing the [`Rewriter`](@ref mim::Rewriter) traversal machinery.

It inherits from both [`Phase`](@ref mim::Phase) and [`Rewriter`](@ref mim::Rewriter), but unlike a rewriting phase, it rewrites **into the same world**. In practice, this means it uses [`Rewriter`](@ref mim::Rewriter) as a structured recursive traversal over ordinary MimIR terms.

This point is important:

> An [`Analysis`](@ref mim::Analysis) based on [`Rewriter`](@ref mim::Rewriter) has a domain of ordinary [`Def`](@ref mim::Def)s.

As a consequence, analysis information can itself be represented as regular MimIR values. This means that all usual IR mechanisms apply automatically, including:

- hash-consing,
- built-in normalizations,
- canonical sharing,
- and any other simplifications already provided by the [`World`](@ref mim::World).

In other words, an analysis does not need a separate abstract value language unless it really wants one. If your abstract domain fits naturally into MimIR, you can often encode it directly as [`Def`](@ref mim::Def), and the existing IR infrastructure will do a lot of work for free.

An [`Analysis`](@ref mim::Analysis) visits:

1. all registered annex roots, then
2. all external mutables

During the first part, [`mim::Analysis::is_bootstrapping()`](@ref mim::Analysis::is_bootstrapping) is `true`.
During the second part, it is `false`.

The intended use is:

- override [`rewrite`](@ref mim::Rewriter::rewrite), [`rewrite_imm`](@ref mim::Rewriter::rewrite_imm), [`rewrite_mut`](@ref mim::Rewriter::rewrite_mut), or node-specific rewrite hooks,
- compute analysis information while traversing reachable IR,
- store that information in your own side tables,
- set `todo_ = true` if the analysis discovered new information and should be rerun in a fixed-point loop.

### Powered by Rewriter

The key idea is:

> [`Analysis`](@ref mim::Analysis) uses the rewriting infrastructure as a graph-aware traversal engine over real MimIR nodes.

You get:
- memoization,
- correct handling of mutables and cycles,
- node-wise customization hooks,
- scoped traversal behavior,
- and a domain that can be expressed in ordinary [`def`](@ref mim::Def)s.

That last point is often very convenient for analyses: abstract values can reuse MimIR structure instead of inventing a parallel representation.

### Reset Between Iterations

If an analysis participates in a fixed-point loop, it should be ready to run multiple times.
The base [`reset()`](@ref mim::Analysis::reset) clears the rewriter state and resets `todo_` for the next round.

## RWPhase {#phases_rwphase}

[`RWPhase`](@ref mim::RWPhase) is the base class for phases that **rebuild the current world into a new one**.
This is the standard base class for optimization phases that structurally transform IR.
It inherits from both [`Phase`](@ref mim::Phase) and [`Rewriter`](@ref mim::Rewriter), but here the two worlds differ:
- [`Phase::world`](@ref mim::Stage::world) is the **old** world,
- [`Rewriter::world`](@ref mim::Rewriter::world) is the **new** world.

@note
To avoid confusion, direct `world()` access is deleted.
Use:
- [`old_world()`](@ref mim::RWPhase::old_world) to inspect existing IR,
- [`new_world()`](@ref mim::RWPhase::new_world) to build rewritten IR.

### Execution Model

An [`RWPhase`](@ref mim::RWPhase) runs in three conceptual steps:

1. optionally perform a fixed-point analysis on the old world,
2. rewrite all old [`def`](@ref mim::Def)s into the new world:
   1. rewrite all annex roots into the new world,
   2. rewrite all external mutables into the new world;
3. swap old and new worlds.

After the swap, the rewritten world becomes the current one.

So an [`RWPhase`](@ref mim::RWPhase) is the standard pattern for whole-world transformations:

> analyze the old program, rebuild the transformed program, then replace the old world.

### Cleanup

The [`Cleanup`](@ref mim::Cleanup) phase is simply an [`RWPhase`](@ref mim::RWPhase) that performs no custom rewrites.
Since an [`RWPhase`](@ref mim::RWPhase) only reconstructs what is reachable from the world roots, rebuilding the old world into a new one automatically eliminates garbage, i.e. dead and unreachable code.

### Bootstrapping

Like [`Analysis`](@ref mim::Analysis), [`RWPhase`](@ref mim::RWPhase) first processes annex roots and only afterwards externals.

While annexes are being rewritten, [`mim::RWPhase::is_bootstrapping()`](@ref mim::RWPhase::is_bootstrapping) is `true`.

This matters because annexes may depend on one another. During bootstrapping, rewrites that require other annexes to already exist in the new world may need to be deferred or skipped.

### Optional Pre-Analysis

An [`RWPhase`](@ref mim::RWPhase) may be given an associated [`Analysis`](@ref mim::Analysis).
If so, [`analyze()`](@ref mim::RWPhase::analyze) runs that analysis to a fixed point before the actual rewrite begins.

This is a common pattern:

- analysis computes facts on the old world,
- rewrite uses those facts to produce a transformed new world.

If no analysis is needed, [`analyze()`](@ref mim::RWPhase::analyze) can just return `false`.

### Typical Shape

```cpp
class MyRWPhase : public mim::RWPhase {
public:
    MyRWPhase(World& world)
        : RWPhase(world, "my_rw_phase") {}

private:
    const Def* rewrite_imm_App(const App* app) override {
        // customize rebuilding here
        return Rewriter::rewrite_imm_App(app);
    }
};
```

Run it with:

```cpp
mim::Phase::run<MyRWPhase>(world);
```

## PhaseMan {#phases_phase_man}

[`PhaseMan`](@ref mim::PhaseMan) organizes several phases into a pipeline.

It can run them:

- once, in sequence, or
- repeatedly to a fixed point

A fixed-point [`PhaseMan`](@ref mim::PhaseMan) reruns the whole pipeline as long as at least one phase sets `todo_ = true`.

Between iterations, each phase is recreated from its original configuration. This keeps phase-local state from leaking across rounds unless the phase explicitly recomputes it.

Conceptually:

> [`PhaseMan`](@ref mim::PhaseMan) is the orchestration layer for classical phase pipelines.

### Typical Usage

```cpp
auto phases = mim::Phases();
phases.emplace_back(std::make_unique<PhaseA>(world));
phases.emplace_back(std::make_unique<PhaseB>(world));

mim::PhaseMan man(world, mim::plug::compile::phases);
man.apply(/*fixed_point=*/true, std::move(phases));
man.run();
```

Use a fixed-point pipeline when phases expose new optimization opportunities for one another.

## ClosedMutPhase {#phases_closed_mut_phase}

[`ClosedMutPhase`](@ref mim::ClosedMutPhase) is a traversal helper for phases that want to visit all reachable, **closed** mutables in the world.

A mutable is considered relevant here if it is:

- reachable,
- closed, i.e. has no free variables,
- optionally non-empty depending on `elide_empty`.

This is useful for local analyses or transformations that are naturally phrased as:

> for every reachable closed mutable, inspect or process it.

You override `visit(M*)`, where `M` defaults to [`Def`](@ref mim::Def) but may be restricted to a particular mutable subtype.

### Typical Shape

```cpp
class MyClosedPhase : public mim::ClosedMutPhase<Lam> {
public:
    MyClosedPhase(World& world)
        : ClosedMutPhase(world, "my_closed_phase", /*elide_empty=*/true) {}

private:
    void visit(Lam* lam) override {
        // process each reachable closed Lam
    }
};
```

## NestPhase {#phases_nest_phase}

[`NestPhase`](@ref mim::NestPhase) builds on [`ClosedMutPhase`](@ref mim::ClosedMutPhase) and computes a [`Nest`](@ref mim::Nest) object for each visited mutable.

Use it when your phase needs a structured view of nested control or binding structure rather than just the raw mutable.

Instead of overriding `visit(M*)`, you override:

```cpp
visit(const Nest&)
```

This is convenient for analyses that reason about nesting, dominance-like structure, or hierarchical regions.

## Example: SCCP

\include "examples/sccp.h"

The provided [Sparse Conditional Constant Propagation (SCCP)](https://en.wikipedia.org/wiki/Sparse_conditional_constant_propagation) implementation is a good example of how phases are intended to be structured.
Its overall architecture is:
- an inner [`Analysis`](@ref mim::Analysis) computes propagation facts on the old world,
- the outer [`RWPhase`](@ref mim::RWPhase) uses those facts to rebuild a simplified new world.

@note The implementation does not only propagate constants but arbitrary expressions!


### What the Analysis Computes

\include "examples/sccp.cpp"

The SCCP analysis associates each lambda variable with a lattice value:
- bottom: no useful information yet,
- a concrete expression: this value can be propagated,
- top: keep the variable as-is.

In the implementation, this lattice is stored as a `Def2Def` map.
A particularly nice aspect here is that the propagated value is itself a regular [`Def`](@ref mim::Def).
This is exactly the benefit of building analysis on top of [`Rewriter`](@ref mim::Rewriter): the abstract domain can live directly inside MimIR, so canonicalization and normalization come for free.

The analysis traverses the old world and updates the lattice when it sees applications of optimizable lambdas. If new information is discovered, it sets `todo_ = true`, causing the analysis to rerun until stable.
This is a textbook use of [`Analysis`](@ref mim::Analysis):
- walk the old IR,
- collect facts,
- iterate to a fixed point.

### What the Rewrite Does

Once the lattice is stable, the outer SCCP phase starts rewriting.
When it sees an application of a lambda whose parameters have propagated values, it rebuilds a specialized lambda:
- parameters with known propagated expressions are removed,
- remaining parameters are kept,
- the lambda body is rewritten with the propagated values substituted,
- the call site is rebuilt with only the remaining arguments.

So SCCP uses the standard [`RWPhase`](@ref mim::RWPhase) pattern:
1. analyze the old world,
2. rewrite into a new world using the computed facts,
3. swap worlds.

### Why This Split Is Useful

Separating SCCP into analysis and rewrite keeps both parts simple:
- the analysis never mutates or partially rewrites the program,
- the rewrite does not need to discover facts on the fly,
- fixed-point logic stays in the analysis stage where it belongs.

This separation is the main design pattern to follow for nontrivial optimizations.

@note
The complete SCCP example fits into roughly 150 lines of C++ source code.
Most of the usual compiler boilerplate is absorbed by the existing [Analysis](@ref mim::Analysis), [RWPhase](@ref mim::RWPhase), and [Rewriter](@ref mim::Rewriter) infrastructure, so the implementation can focus on the optimization itself.

## Choosing the Right Base Class

A useful rule of thumb is:
- derive from [`Phase`](@ref mim::Phase) if you just need a custom one-off action,
- derive from [`Analysis`](@ref mim::Analysis) if you want a graph-aware traversal that computes facts on the current world,
- derive from [`RWPhase`](@ref mim::RWPhase) if you want to rebuild the world into a transformed new one,
- derive from [`ClosedMutPhase`](@ref mim::ClosedMutPhase) if you want to visit all reachable closed mutables,
- derive from [`NestPhase`](@ref mim::NestPhase) if that visit should come with a computed [`Nest`](@ref mim::Nest).

## Recommended Design Pattern

For most optimization phases, the preferred structure is:
1. write an [`Analysis`](@ref mim::Analysis) that computes facts to a fixed point,
2. write an [`RWPhase`](@ref mim::RWPhase) that consumes those facts while rebuilding the world.

This keeps analyses and transformations cleanly separated and fits naturally with MimIR’s rewriting-based infrastructure.

## Minimal Examples

A simple whole-world rewrite phase:

```cpp
class Simplify : public mim::RWPhase {
public:
    Simplify(mim::World& world)
        : RWPhase(world, "simplify") {}

private:
    const mim::Def* rewrite_imm_App(const mim::App* app) override {
        // rewrite or simplify selected applications
        // fallback:
        return Rewriter::rewrite_imm_App(app);
    }
};
```

A simple analysis phase:

```cpp
class CountMutLams : public mim::Analysis {
public:
    CountMutLams(mim::World& world)
        : Analysis(world, "count_lams") {}

    size_t num_lams = 0;

private:
    const mim::Def* rewrite_mut_Lam(mim::Lam* lam) override {
        ++num_lams;
        return mim::Analysis::rewrite_mut_Lam(lam);
    }
};
```

Using both:

```cpp
CountMutLams analysis(world);
analysis.run();

mim::Phase::run<Simplify>(world);
```

## Summary

Phases are MimIR’s main unit of compiler work.
- [`Phase`](@ref mim::Phase) is the minimal base abstraction.
- [`Analysis`](@ref mim::Analysis) is for graph-aware fact collection on the current world.
- [`RWPhase`](@ref mim::RWPhase) is for rewriting the current world into a transformed new one.
- [`PhaseMan`](@ref mim::PhaseMan) sequences phases, optionally to a fixed point.
- [`ClosedMutPhase`](@ref mim::ClosedMutPhase) and [`NestPhase`](@ref mim::NestPhase) are traversal helpers for common whole-world inspections.

The key design idea is that MimIR phases are built around structured world traversal and rewriting. For substantial optimizations, the usual pattern is:
- compute facts with [`Analysis`](@ref mim::Analysis),
- consume them with [`RWPhase`](@ref mim::RWPhase).
