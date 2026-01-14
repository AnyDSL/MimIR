#pragma once

#include "mim/phase.h"

namespace mim::plug::mem::phase {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class SplitKernel : public RWPhase {
public:
    SplitKernel(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    void start() final;
    bool analyze() final;
    void analyze(const Def*);

    const Def* rewrite_imm_App(const App*) final;
    const Def* rewrite_mut_Lam(Lam*) final;

    DefSet analyzed_;
    LamSet kernels_;
};

} // namespace mim::plug::mem::phase
