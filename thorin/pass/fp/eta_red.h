#ifndef THORIN_PASS_FP_ETA_RED_H
#define THORIN_PASS_FP_ETA_RED_H

#include "thorin/pass/pass.h"

namespace thorin {

/// Performs η-reduction.
/// Rewrites `λx.e x` to `e`, whenever `x` does (optimistically) not appear free in `e`.
class EtaRed : public FPPass<EtaRed> {
public:
    EtaRed(PassMan& man)
        : FPPass(man, "eta_red") {}

    enum Lattice {
        Bot,         ///< Never seen.
        Reduce,      ///< η-reduction performed.
        Irreducible, ///< η-reduction not possible as we stumbled upon a Var.
    };

    using Data = LamMap<Lattice>;
    void mark_irreducible(Lam* lam) { irreducible_.emplace(lam); }

private:
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Var*) override;

    LamSet irreducible_;
};

}

#endif
