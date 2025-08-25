#pragma once

#include "mim/phase/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class BranchNormalizePhase : public RWPhase {
public:
    BranchNormalizePhase(World& world)
        : RWPhase(world, "branch normalize") {}

private:
    const Def* rewrite_mut_Lam(Lam*) final;

    DefSet analyzed_;
    LamMap<bool> candidate_;
};

} // namespace mim
