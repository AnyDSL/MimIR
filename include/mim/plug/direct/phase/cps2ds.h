#pragma once

#include "mim/def.h"
#include "mim/phase.h"
#include "mim/rewrite.h"

namespace mim::plug::direct {

/// Rewrite direct.cps2ds to cps.
class CPS2DSPhase : public Phase {
public:
    CPS2DSPhase(World& world, flags_t annex)
        : Phase(world, annex)
        , rw_(world) {}

    void start();

private:
    bool analyze();
    const Def* init(Def*);
    const Def* init(const Def*);

    const Def* rewrite(const Def* def);

    Lam* make_continuation(const Def* body, const Def* cn_type, const Def* arg, Sym prefix);
    Lam* curr_lam() const { return lamStack_.empty() ? nullptr : lamStack_.back(); }

    DefSet visited_;
    Lam2Lam lam2lam_;
    Rewriter rw_;
    Def2Def rewritten_;
    Vector<Lam*> lamWL_;
    Vector<Lam*> lamStack_;
};

} // namespace mim::plug::direct
