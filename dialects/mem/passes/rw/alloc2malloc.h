#pragma once

#include "thorin/pass/pass.h"

namespace thorin::mem {

class Alloc2Malloc : public RWPass<Alloc2Malloc, Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::mem
