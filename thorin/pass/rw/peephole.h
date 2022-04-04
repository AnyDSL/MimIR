#ifndef THORIN_PASS_PEEPHOLE_H
#define THORIN_PASS_PEEPHOLE_H

#include "thorin/pass/pass.h"

namespace thorin {

class Peephole : public RWPass<Lam> {
public:
    Peephole(PassMan& man)
        : RWPass(man, "peephole") {}

    const Def* rewrite(const Def*) override;
};

}

#endif
