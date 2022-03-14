#ifndef THORIN_PASS_FP_ETA_EXP_H
#define THORIN_PASS_FP_ETA_EXP_H

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
    const Proxy* proxy(Lam*);
    void new2old(Lam* new_lam, Lam* old_lam) { new2old_[new_lam] = old_lam; }
    Lam* new2old(Lam* new_lam);
    ///@}

    /// @name lattice
    ///@{
    /**
     * ```
     *       expand_            <-- η-expand non-callee as it occurs more than once; don't η-reduce the wrapper again.
     *        /   \
     *  Callee     Non_Callee_1 <-- Multiple callees XOR exactly one non-callee are okay.
     *        \   /
     *         Bot              <-- Never seen.
     * ```
     */
    enum Lattice : bool { Callee, Non_Callee_1 };
    static std::string_view lattice2str(Lattice l) { return l == Callee ? "Callee" : "Non_Callee_1"; }
    ///@}

    using Data = std::tuple<Def2Def, LamMap<Lattice>>;

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(const Def*) override;
    ///@}

    /// @name helpers
    ///@{
    Lam* eta_wrap(Lam*);
    ///@}

    EtaRed* eta_red_;
    LamSet expand_;
    Lam2Lam wrap2orig_;
    Lam2Lam new2old_;
    DefMap<Array<const Def*>> def2new_ops_;
};

} // namespace thorin

#endif
