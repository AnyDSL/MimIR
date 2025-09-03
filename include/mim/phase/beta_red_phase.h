#pragma once

#include "mim/phase/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class BetaRedPhase : public FPPhase {
public:
    BetaRedPhase(World&);

private:
    void reset() final;
    bool analyze() final;
    void analyze(const Def*);
    void visit(Lam*);

    const Def* rewrite_imm_App(const App*) final;
    bool is_candidate(Lam*) const;

    DefSet analyzed_;
    LamMap<bool> candidates_;
};

} // namespace mim
