#ifndef THORIN_PASS_RW_LOWER_FOR_H
#define THORIN_PASS_RW_LOWER_FOR_H

#include "thorin/pass/pass.h"

namespace thorin {


/// Lowers the for axiom to actual control flow in CPS style
class LowerFor : public RWPass<Lam> {
public:
    LowerFor(PassMan& man)
        : RWPass(man, "lower_affine_for") {}

    const Def* rewrite(const Def*) override;

private:
};

}

#endif
