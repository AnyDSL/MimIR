
#ifndef THORIN_PASS_ETA_CONT_H
#define THORIN_PASS_ETA_CONT_H

#include "thorin/pass/pass.h"

namespace thorin {

class EtaCont : public RWPass<Lam> {
public:
    EtaCont(PassMan& man)
        : RWPass(man, "eta_cont"), old2wrapper_() {}

    
    const Def* eta_wrap_cont(const Def*);
    const Def* rewrite(const Def*) override;

private:
    DefMap<Lam*> old2wrapper_;
};

}

#endif
