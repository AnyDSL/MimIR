#ifndef THORIN_PASS_RW_ALLOC2MALLOC_H
#define THORIN_PASS_RW_ALLOC2MALLOC_H

#include "thorin/pass/pass.h"

namespace thorin {

class Alloc2Malloc : public RWPass<Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc")
    {}

    const Def* rewrite(const Def*) override;
};

}

#endif
