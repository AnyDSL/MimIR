#pragma once

#include "mim/pass.h"

namespace mim {

/// Optimistically performs β-reduction (aka inlining).
/// β-reduction of `f e` happens if `f` only occurs exactly once in the program in callee position.
class BetaRed : public FPPass<BetaRed, Def> {
public:
    BetaRed(PassMan& man)
        : FPPass(man, "beta_red") {}

    using Data = LamSet;

    void keep(Lam* lam) { keep_.emplace(lam); }

private:
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(const Def*) override;

    LamSet keep_;
};

} // namespace mim
