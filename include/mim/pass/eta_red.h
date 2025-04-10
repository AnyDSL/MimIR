#pragma once

#include "mim/pass/pass.h"

namespace mim {

/// Performs η-reduction.
/// Rewrites `λx.e x` to `e`, whenever `x` does (optimistically) not appear free in `e`.
class EtaRed : public FPPass<EtaRed, Def> {
public:
    EtaRed(PassMan& man, bool callee_only = false)
        : FPPass(man, "eta_red")
        , callee_only_(callee_only) {}

    enum Lattice {
        Bot,         ///< Never seen.
        Reduce,      ///< η-reduction performed.
        Irreducible, ///< η-reduction not possible as we stumbled upon a Var.
    };

    using Data = LamMap<Lattice>;
    void mark_irreducible(Lam* lam) { irreducible_.emplace(lam); }

private:
    const bool callee_only_;
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Var*) override;

    LamSet irreducible_;
};

} // namespace mim
