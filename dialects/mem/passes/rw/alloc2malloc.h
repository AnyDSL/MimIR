#pragma once

#include "thorin/pass/pass.h"

namespace thorin::mem {

class Alloc2Malloc : public RWPass<Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();
};

} // namespace thorin::mem
