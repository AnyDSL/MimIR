#pragma once

#include "mim/pass/pass.h"

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
    CopyProp(PassMan& man, bool bb_only = false);

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

    BetaRed* beta_red_;
    EtaExp* eta_exp_;
    LamMap<std::tuple<Lattices, Lam*, DefVec>> lam2info_;
    const bool bb_only_;
};

} // namespace plug::mem
} // namespace mim
