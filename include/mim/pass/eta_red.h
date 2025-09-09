#pragma once

#include <memory>

#include "mim/pass.h"

namespace mim {

/// Performs η-reduction.
/// Rewrites `λx.e x` to `e`, whenever `x` does (optimistically) not appear free in `e`.
class EtaRed : public FPPass<EtaRed, Def> {
public:
    EtaRed(World& world, flags_t annex, bool callee_only)
        : FPPass(world, annex)
        , callee_only_(callee_only) {}

    std::unique_ptr<Stage> recreate() final { return std::make_unique<EtaRed>(world(), annex(), callee_only()); }

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
