#pragma once

#include "mim/phase/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class BetaEtaRedPhase : public FPPhase {
public:
    BetaEtaRedPhase(World& world)
        : FPPhase(world, "beta reduction") {}

    void inc(Lam* lam) {
        if (auto [i, ins] = candidate_.emplace(lam, false); !ins) i->second = false;
    }

    bool is_candidate(Lam* lam) const {
        auto i = candidate_.find(lam);
        assert(i != candidate_.end());
        return i->second;
    }

    bool analyze() final;

private:
    void analyze(const Def*);
    const Def* rewrite_imm_App(const App*) final;

    DefSet analyzed_;
    LamMap<bool> candidate_;
};

} // namespace mim
