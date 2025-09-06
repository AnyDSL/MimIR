#pragma once

#include "mim/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class BranchNormalizePhase : public RWPhase {
public:
    BranchNormalizePhase(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    const Def* rewrite_mut_Lam(Lam*) final;

    DefSet analyzed_;
    LamMap<bool> candidate_;
};

} // namespace mim
