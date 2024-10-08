#pragma once

#include "mim/pass/pass.h"

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
    Ref rewrite(Ref) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(Ref) override;

    LamSet keep_;
};

} // namespace mim
