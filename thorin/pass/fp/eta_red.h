#ifndef THORIN_PASS_FP_ETA_RED_H
#define THORIN_PASS_FP_ETA_RED_H

#include "thorin/pass/pass.h"

namespace thorin {

/// Performs η-reduction.
/// Rewrites `λx.e x` to `e`, whenever `x` does (optimistically) not appear free in `e`.
class EtaRed : public FPPass<EtaRed> {
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

    static PassTag* ID();

private:
    const bool callee_only_;
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Var*) override;

    LamSet irreducible_;
};

} // namespace thorin

#endif