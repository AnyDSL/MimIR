#pragma once

#include "mim/pass.h"

namespace mim {

class BetaRed;
class EtaExp;

namespace plug::mem {

/// This FPPass is similar to sparse conditional constant propagation (SCCP).
/// However, this optmization also works on all Lam%s alike and does not only consider basic blocks as opposed to
/// traditional SCCP. What is more, this optimization will also propagate arbitrary Def%s and not only constants.
/// Finally, it will also remove dead Var%s.
class CopyProp : public FPPass<CopyProp, Lam> {
public:
    CopyProp(World& world, flags_t annex, bool bb_only, BetaRed* br, EtaExp* ee)
        : FPPass(world, annex)
        , bb_only_(bb_only)
        , beta_red_(br)
        , eta_exp_(ee) {}

    CopyProp* recreate() final { return driver().stage<CopyProp>(world(), annex(), bb_only(), beta_red_, eta_exp_); }

    bool bb_only() const { return bb_only_; }

    using Data = LamMap<DefVec>;

private:
    /// Lattice used for this Pass:
    /// ```
    ///  Keep    <-- We cannot do anything here.
    ///   |
    ///  Prop    <-- Var is live but the same Def everywhere.
    ///   |
    ///  Dead    <-- Var is dead.
    /// ```
    enum Lattice : u8 { Dead, Prop, Keep };
    enum : u32 { Varxy, Appxy };
    using Lattices = Vector<Lattice>;

    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    ///@}

    bool bb_only_;
    BetaRed* beta_red_;
    EtaExp* eta_exp_;
    LamMap<std::tuple<Lattices, Lam*, DefVec>> lam2info_;
};

} // namespace plug::mem
} // namespace mim
