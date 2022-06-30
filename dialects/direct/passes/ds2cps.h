#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

class DS2CPS : public RWPass<Lam> {
public:
    DS2CPS(PassMan& man)
        : RWPass(man, "ds2cps") {}

    void enter() override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
    Lam* currentLambda=nullptr;

    void rewrite_lam(Lam* lam);
    const Def* rewrite_(const Def*);
    const Def* rewrite_inner(const Def*);
};

} // namespace thorin::direct
