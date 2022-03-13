#ifndef THORIN_PASS_FP_COPY_PROP_H
#define THORIN_PASS_FP_COPY_PROP_H

#include "thorin/pass/pass.h"

namespace thorin {

class BetaRed;
class EtaExp;

/// This FPPass is similar to sparse conditional constant propagation (SCCP).
/// However, this optmization also works on all Lam%s alike and does not only consider basic blocks as opposed to
/// traditional SCCP. What is more, this optimization will also propagate arbitrary Def%s and not only constants.
/// Finally, it will also remove dead Var%s.
class CopyProp : public FPPass<CopyProp, Lam> {
public:
    CopyProp(PassMan& man, BetaRed* beta_red, EtaExp* eta_exp, bool bb_only = false)
        : FPPass(man, "copy_prop")
        , beta_red_(beta_red)
        , eta_exp_(eta_exp) 
        , bb_only_(bb_only){}

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
    enum : flags_t { Varxy, Appxy };

    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    ///@}

    BetaRed* beta_red_;
    EtaExp* eta_exp_;
    LamMap<std::tuple<Array<Lattice>, Lam*, DefArray>> lam2info_;
    const bool bb_only_;
};

} // namespace thorin

#endif
