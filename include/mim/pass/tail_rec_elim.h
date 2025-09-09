#pragma once

#include "mim/pass.h"

namespace mim {

class EtaRed;

class TailRecElim : public FPPass<TailRecElim, Lam> {
public:
    TailRecElim(World& world, flags_t annex, EtaRed* er)
        : FPPass(world, annex)
        , eta_red_(er) {}
    TailRecElim* recreate() final { return driver().stage<TailRecElim>(world(), annex(), eta_red_); }

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Def*) override;
    ///@}

    EtaRed* eta_red_;
    LamMap<std::pair<Lam*, Lam*>> old2rec_loop_;
    Lam2Lam rec2loop_;
};

} // namespace mim
