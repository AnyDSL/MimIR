# Passes

[TOC]

There are two kind of passes in Thorin:
1. Rewrite Pass
2. Fixed-Point Pass

## Rewrite Pass

In order to write a rewrite pass, you have to inherit from [RWPass](@ref thorin::RWPass).
Usually, you are only interested in looking for code patterns that only occur in specific nominals - typically `Lam`bdas.
This is the template argument in order to inspect only specific nominals.
The main hook, the [PassMan](@ref thorin::PassMan) provides, is the [rewrite](@ref thorin::RWPassBase::rewrite) method.
As an example, let's have a look at the [Alloc2Malloc](@ref thorin::Alloc2Malloc) pass.
It rewrites `alloc`/`slot` calls into their more verbose siblings `malloc`/`mslot` that make the size of the alloc'ed size explicit:
This is `alloc2malloc.h`:
```cpp
#ifndef THORIN_PASS_RW_ALLOC2MALLOC_H
#define THORIN_PASS_RW_ALLOC2MALLOC_H

#include "thorin/pass/pass.h"

namespace thorin {

class Alloc2Malloc : public RWPass<Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc")
    {}

    const Def* rewrite(const Def*) override;
};

}

#endif
```

The actual `rewrite` simply inspects the current `def`.
If this happens to be a `alloc`/`slot`, it will simply return the more explicit counterpart.
This is `alloc2malloc.cpp`:
```cpp
#include "thorin/pass/rw/alloc2malloc.h"

namespace thorin {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    if (auto alloc = isa<Tag::Alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return world().op_malloc(pointee, alloc->arg(), alloc->dbg());
    } else if (auto slot  = isa<Tag::Slot >(def)) {
        auto [pointee, addr_space] = slot->decurry()->args<2>();
        auto [mem, id] = slot->args<2>();
        return world().op_mslot(pointee, mem, id, slot->dbg());
    }

    return def;
}

}
```

## Fixed-Point Pass

A fixed-point pass allows you to implement a data-flow analysis.
Traditionally, an optimizations performs
1. the analysis and
2. runs the actual transformation based upon the analysis info.

A fixed-point pass, however, will directly rewrite the code.
You **optimistically** assume that all your assumptions your pass could possibly make about the program simply hold.
Now, two things might happen:
1. You do not run into any contradictions later on, so your optimistic assumptions were in fact sound.
2. You prove yourself wrong later on.

If you find a contradiction to your initial assumption, you have to
1. undo you mistake and backtrace and
2. correct your assumption such that you do not do the same mistake over and over again.

In order to write a fixed-point pass, you have to inherit from [FPPass](@ref thorin::FPPass).
It expects two template parameters:
1. This must be the very class you are currently implementing (known as [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern).
2. This is the same as for rewrite passes and will most likely be `Lam`.

### Rewrite

TODO

### Analyze

TODO

## Other Hooks

TODO
