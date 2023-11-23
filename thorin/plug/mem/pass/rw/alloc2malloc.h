#pragma once

#include "thorin/pass/pass.h"

namespace thorin::plug::mem {

class Alloc2Malloc : public RWPass<Alloc2Malloc, Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::plug::mem
