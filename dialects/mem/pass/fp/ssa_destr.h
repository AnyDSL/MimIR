#pragma once

#include "thorin/phase/phase.h"

namespace thorin::mem {

class SSADestr : public FPPass<SSADestr, Lam> {
public:
    SSADestr(PassMan& man)
        : FPPass(man, "ssa_destr") {}

    using Data = DefMap<int>;

private:
    /// @name PassMan hooks
    ///@{
    void enter() override;
    Ref rewrite(const Proxy*) override;
    Ref rewrite(Ref) override;
    undo_t analyze(const Proxy*) override;
    undo_t analyze(Ref) override;
    ///@}
    ///

    Ref mem_;
};

} // namespace thorin::mem
