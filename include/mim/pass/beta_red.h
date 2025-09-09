#pragma once

#include "mim/pass.h"

namespace mim {

/// Optimistically performs β-reduction (aka inlining).
/// β-reduction of `f e` happens if `f` only occurs exactly once in the program in callee position.
class BetaRed : public FPPass<BetaRed, Def> {
public:
    BetaRed(World& world, flags_t annex)
        : FPPass(world, annex) {}
    BetaRed* recreate() final { return driver().stage<BetaRed>(world(), annex()); }

    using Data = LamSet;

    void keep(Lam* lam) { keep_.emplace(lam); }

private:
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(const Def*) override;

    LamSet keep_;
};

} // namespace mim
