#ifndef THORIN_PHASE_RW_ADD_MEM_H
#define THORIN_PHASE_RW_ADD_MEM_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>
#include <thorin/phase/phase.h>

namespace thorin::mem {

/// Adds an additional memory argument to all functions.
/// If a memory argument is already present, the signature is not changed.
/// Otherwise, the signatures `[[..args], ret]` and `[..ret]` are converted to `[[mem,[..args]], ret']` and
/// `[mem,..ret]` respectively. An additional pass is required to ensure that functions always contain their memory at
/// position 1. Functions are converted recursively. Calls (in CPS position) are augmented with an additional memory
/// argument if not present. The memory is tracked and propagated through the program.
/// The current memory is the last output of an operation (often as part of a tuple).
/// The recursion order proceeds from left to right, bottom up with recursive descent and following ascend.

class AddMem : public RWPhase {
public:
    AddMem(World& world)
        : RWPhase(world, "add_mem") {}

    const Def* rewrite_structural(const Def*) override;

    static PassTag* ID();

private:
    Def2Def rewritten;
};

} // namespace thorin::mem

#endif
