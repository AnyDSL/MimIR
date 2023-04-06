#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class EtaRed;

class TailRecElim : public FPPass<TailRecElim, Lam> {
public:
    TailRecElim(PassMan& man, EtaRed* eta_red = nullptr)
        : FPPass(man, "tail_rec_elim")
        , eta_red_(eta_red) {}

private:
    /// @name PassMan hooks
    ///@{
    Ref rewrite(Ref) override;
    undo_t analyze(Ref) override;
    ///@}

    EtaRed* eta_red_;
    LamMap<std::pair<Lam*, Lam*>> old2rec_loop_;
    Lam2Lam rec2loop_;
};

} // namespace thorin
