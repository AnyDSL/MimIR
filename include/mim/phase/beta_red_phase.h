#pragma once

#include "mim/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class BetaRedPhase : public FPPhase {
public:
    BetaRedPhase(World& world, flags_t annex)
        : FPPhase(world, annex) {}
    std::unique_ptr<Stage> recreate() final { return std::make_unique<BetaRedPhase>(old_world(), annex()); }

private:
    bool analyze() final;
    void analyze(const Def*);
    void visit(const Def*);

    const Def* rewrite_imm_App(const App*) final;
    bool is_candidate(Lam*) const;

    DefSet analyzed_;
    LamMap<bool> candidates_;
};

} // namespace mim
