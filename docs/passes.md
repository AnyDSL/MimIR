# Passes {#passes}

[TOC]

Passes in Thorin are the main infrastructure and preferred method when implementing code transformations.
However, nothing stops you from manually iterating and transforming the program.
This is usually only necessary if you need a very specific iteration behavior for your transformation.

Passes are managed by the [PassMan](@ref thorin::PassMan)ager.
You can put together your optimization pipeline like so:
```cpp
    PassMan opt(world);
    opt.add<PartialEval>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<Scalerize>(ee);
    opt.add<CopyProp>(br, ee);
    opt.run();
```
Note how some passes depend on other passes.
For example, the [CopyProp](@ref thorin::plug::mem::CopyProp)agation depends on the [BetaRed](@ref thorin::BetaRed)uction and [EtaExp](@ref thorin::EtaExp)ansion.
In contrast to traditional passes in compilers, Thorin's [PassMan](@ref thorin::PassMan) will run all passes in tandem and combine the obtained results into the most optimal solution and, hence, avoid the dreaded *phase-ordering problem*.

There are two kind of passes in Thorin:
1. [Rewrite Pass](@ref thorin::RWPass)
2. [Fixed-Point Pass](@ref thorin::FPPass)

## Rewrite Pass

In order to write a [rewrite pass](@ref thorin::RWPass), you have to inherit from [RWPass](@ref thorin::RWPass).
Usually, you are only interested in looking for code patterns that only occur in specific mutables - typically [Lam](@ref thorin::Lam)bdas.
You can filter for these mutables by passing it as template parameter to [RWPass](@ref thorin::RWPass) when inherting from it.
The main hook to the [PassMan](@ref thorin::PassMan), is the [rewrite](@ref thorin::Pass::rewrite) method.
As an example, let's have a look at the [Alloc2Malloc](@ref thorin::plug::mem::Alloc2Malloc) pass.
It rewrites `alloc`/`slot` calls into their more verbose siblings `malloc`/`mslot` that make the size of the alloc'ed type explicit:
This is `alloc2malloc.h`:
\include "thorin/plug/mem/pass/alloc2malloc.h"

The actual `rewrite` simply inspects the current `def`.
If this happens to be a `alloc`/`slot`, it will simply return the more explicit counterpart.
This is `alloc2malloc.cpp`:
\include "thorin/plug/mem/pass/alloc2malloc.cpp"

## Fixed-Point Pass

A [fixed-point pass](@ref thorin::FPPass) allows you to implement a data-flow analysis.
Traditionally, an optimizations performs
1. the analysis and
2. runs the actual transformation based upon the analysis info.

A [fixed-point pass](@ref thorin::FPPass), however, will directly rewrite the code.
You **optimistically** assume that all your assumptions your pass could possibly make about the program simply hold.
Now, two things might happen:
1. You do not run into any contradictions later on, so your optimistic assumptions were in fact sound.
2. You prove yourself wrong later on.

If you find a contradiction to your initial assumption, you have to
1. undo your mistake by backtracing to the place where you erroneously made the wrong decision and
2. correct your assumption such that you do not make the same mistake over and over again.

In order to write a [fixed-point pass](@ref thorin::FPPass), you have to inherit from [FPPass](@ref thorin::FPPass).
This pass expects two template parameters.
The first one must be the very class you are currently implementing (this is known as [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) in C++).
The second one has the same purpose as the template parameter for [rewrite passes](@ref thorin::RWPass) and will most likely be [Lam](@ref thorin::Lam).

### Rewrite

The `rewrite` works exactly as for [rewrite passes](@ref thorin::RWPass).
However, this time around, you are guessing that the most optimistic result will happen.
If you prove yourself wrong afterwards, you will gradually ascend in a [lattice](https://en.wikipedia.org/wiki/Complete_lattice) until the [fixed-point pass](@ref thorin::FPPass) stabilizes.
TODO

#### Proxy

### Analyze

TODO

### Other Hooks

## Caveats

### Construct Valid Programs

It is important to always construct a valid program.
Otherwise, bad things might happen as soon as you toss other passes into the mix.
The reason is that the very program you are constructing is the **only** way to communicate with the other passes without directly sharing information.

### Creating Mutables

Eventually, you will need to create mutables during a [fixed-point pass](@ref thorin::FPPass).
You must be super cautious to remember in which exact context you created said mutable.
If you ever come into the same situation again (due to backtracking) you have to make sure that you return the **very same** mutable.
Otherwise, if you create a different mutable, the [PassMan](@ref thorin::PassMan) will most likely diverge as it will constantly backtrack while creating new mutables that the other passes don't know anything about.
