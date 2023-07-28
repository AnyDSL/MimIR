#pragma once

#include "thorin/phase/phase.h"

namespace thorin::mem {

class SSADestr : public FPPass<SSADestr, Lam> {
public:
    SSADestr(PassMan& man)
        : FPPass(man, "ssa_destr") {}

    using Data = GIDNodeMap<Lam*, DefMap<uint32_t>>;

private:
    /// @name PassMan hooks
    ///@{
    void enter() override;
    Ref rewrite(Ref) override;
    Ref rewrite(const Proxy*) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(Ref) override;
    ///@}
    ///

    bool should_destruct(Ref) const;

    Ref phi2mem(const App*, Lam*);

    Ref mem_;
    Lam2Lam phi2mem_;
};

} // namespace thorin::mem
