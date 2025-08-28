#pragma once

#include <mim/pass/pass.h>

namespace mim {

class EtaExp;

namespace plug::mem {

/// SSA construction algorithm that promotes slot%s, load%s, and store%s to SSA values.
/// This is loosely based upon:
/// "Simple and Efficient Construction of Static Single Assignment Form"
/// by Braun, Buchwald, Hack, Leißa, Mallon, Zwinkau.
class SSAConstr : public FPPass<SSAConstr, Lam> {
public:
    SSAConstr(PassMan&);

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
    const Def* rewrite(const Proxy*) override;
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(const Def*) override;
    ///@}

    /// @name SSA construction helpers - see paper
    ///@{
    const Def* get_val(Lam*, const Proxy*);
    const Def* set_val(Lam*, const Proxy*, const Def*);
    const Def* mem2phi(const App*, Lam*);
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

} // namespace plug::mem
} // namespace mim
