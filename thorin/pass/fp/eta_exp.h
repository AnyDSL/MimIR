#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class EtaRed;

/// Performs η-expansion:
/// `f -> λx.f x`, if `f` is a Lam with more than one user and does not appear in callee position.
/// This rule is a generalization of critical edge elimination.
/// It gives other Pass%es such as SSAConstr the opportunity to change `f`'s signature (e.g. adding or removingp Var%s).
class EtaExp : public FPPass<EtaExp, Lam> {
public:
    EtaExp(PassMan& man, EtaRed* eta_red)
        : FPPass(man, "eta_exp")
        , eta_red_(eta_red) {}

    /// @name interface for other passes
    ///@{
    const Proxy* proxy(Lam* lam) { return FPPass<EtaExp, Lam>::proxy(lam->type(), {lam}, 0); }
    void new2old(Lam* new_lam, Lam* old_lam) { new2old_[new_lam] = old_lam; }
    Lam* new2old(Lam* new_lam);
    ///@}

    /// @name Lattice
    ///@{
    /// EtaExp uses the following lattice:
    /// ```
    ///       expand_            <-- η-expand non-callee as the Lam occurs more than once.
    ///        /   \                 Don't η-reduce the expansion again.
    ///  Callee     Non_Callee_1 <-- Pos: Either multiple callees, **or** exactly one non-callee are okay.
    ///        \   /
    ///         Bot              <-- Never seen.
    /// ```
    /// * EtaExp::expand_ trackes if a Lam is the top element.
    /// * If it's not within EtaExp::expand_, it's either `Pos` or `Bot`.
    ///   But there is no need to differentiate this any further.
    /// * EtaExp::Pos tracks the particular element at that level and is memorized statefully.
    enum Pos : bool { Callee, Non_Callee_1 };
    static std::string_view pos2str(Pos pos) { return pos == Callee ? "Callee" : "Non_Callee_1"; }

    using Data = std::tuple<Def2Def, LamMap<Pos>>;
    auto& old2new() { return data<0>(); }
    auto& pos() { return data<1>(); }
    ///@}

private:
    /// @name PassMan hooks
    ///@{
    Ref rewrite(Ref) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(Ref) override;
    ///@}
    Lam* eta_exp(Lam*); ///< Helper that peforms the actual η-expansion.

    EtaRed* eta_red_;
    LamSet expand_;
    Lam2Lam exp2orig_;
    Lam2Lam new2old_;
    DefMap<DefVec> def2new_ops_;
};

} // namespace thorin
