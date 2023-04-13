#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class EtaExp;

namespace mem {

/// SSA construction algorithm that promotes slot%s, load%s, and store%s to SSA values.
/// This is loosely based upon:
/// "Simple and Efficient Construction of Static Single Assignment Form"
/// by Braun, Buchwald, Hack, Leißa, Mallon, Zwinkau.
class SSAConstr : public FPPass<SSAConstr, Lam> {
public:
    SSAConstr(PassMan& man, EtaExp* eta_exp)
        : FPPass(man, "ssa_constr")
        , eta_exp_(eta_exp) {}

    enum : u32 { Phixy, Sloxy, Traxy };

    struct Info {
        Lam* pred = nullptr;
        GIDSet<const Proxy*> writable;
    };

    using Data = GIDNodeMap<Lam*, Info>;

private:
    /// @name PassMan hooks
    ///@{
    void enter() override;
    Ref rewrite(const Proxy*) override;
    Ref rewrite(Ref) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(Ref) override;
    ///@}

    /// @name SSA construction helpers - see paper
    ///@{
    Ref get_val(Lam*, const Proxy*);
    Ref set_val(Lam*, const Proxy*, Ref);
    Ref mem2phi(const App*, Lam*);
    ///@}

    EtaExp* eta_exp_;
    LamMap<std::pair<Lam*, DefVec>> mem2phi_;

    /// Value numbering table.
    GIDNodeMap<Lam*, GIDMap<const Proxy*, const Def*>> lam2sloxy2val_;

    /// Contains the @p Sloxy%s that we need to install as phi in a @c mem_lam to build the @c phi_lam.
    LamMap<std::set<const Proxy*, GIDLt<const Proxy*>>> lam2sloxys_;

    /// Contains @p Sloxy%s we have to keep.
    GIDSet<const Proxy*> keep_;
};

} // namespace mem
} // namespace thorin
