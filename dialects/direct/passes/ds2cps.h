#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

class DS2CPS : public RWPass<Lam> {
public:
    DS2CPS(PassMan& man)
        : RWPass(man, "ds2cps") {}

    const Def* rewrite_(const Def*);
    void enter() override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
    Lam* currentLambda;
};

} // namespace thorin::direct
