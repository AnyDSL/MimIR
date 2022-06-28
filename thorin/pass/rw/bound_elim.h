#pragma once
#        define THORIN_PASS_BOUND_ELIM_H

#        include "thorin/pass/pass.h"

namespace thorin {

class BoundElim : public RWPass {
public:
    BoundElim(PassMan& man)
        : RWPass(man, "bound_elim") {}

    static PassTag* ID();
private:
    const Def* rewrite(Def*, const Def*, const Def*) override;
    const Def* rewrite(const Def*, const Def*, Defs, const Def*) override;
    const Def* rewrite(const Def*) override;
};

}

