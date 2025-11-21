#pragma once

#include "mim/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class SCCP : public RWPhase {
public:
    SCCP(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    bool analyze() final;
    const Def* init(const Def*);
    const Def* concr2abstr(const Def*);
    const Def* concr2abstr_impl(const Def*);
    const Def* join(const Def*, const Def*);

    // const Def* rewrite_imm_App(const App*) final;

    DefSet visited_;
    LamMap<DefVec> lam2vars_;
    Def2Def concr2abstr_;
    bool todo_ = true;
};

} // namespace mim
