#pragma once

#include "mim/phase.h"

namespace mim::plug::mem::phase {

/// Sparse Conditional Constant Propagation.
class SSAPhase : public RWPhase {
public:
    SSAPhase(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    bool analyze() final;
    const Def* init(Def*);
    const Def* init(const Def*);

    std::pair<const Def*, bool> concr2abstr(const Def*, const Def*);
    const Def* concr2abstr(const Def*);
    const Def* concr2abstr_impl(const Def*);
    const Def* join(const Def*, const Def*, const Def*);

    const Def* rewrite_imm_App(const App*) final;

    DefSet visited_;
    Def2Def concr2abstr_;
    Lam2Lam lam2lam_;
    bool todo_ = true;
};

} // namespace mim::plug::mem::phase
