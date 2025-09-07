#pragma once

#include "mim/pass.h"

namespace mim {

/// Performs η-reduction.
/// Rewrites `λx.e x` to `e`, whenever `x` does (optimistically) not appear free in `e`.
class EtaRed : public FPPass<EtaRed, Def> {
public:
    EtaRed(World& world, flags_t annex)
        : FPPass(world, annex) {}

    void apply(bool callee_only);
    void apply(const App* app) final { apply(Lit::as<bool>(app->arg())); }
    void apply(Pass& pass) final { apply(static_cast<EtaRed&>(pass).callee_only()); }

    bool callee_only() const { return callee_only_; }

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
    bool callee_only_;
};

} // namespace mim
