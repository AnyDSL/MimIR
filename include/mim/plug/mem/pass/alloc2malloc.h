#pragma once

#include <mim/pass.h>

namespace mim::plug::mem {

class Alloc2Malloc : public RWPass<Alloc2Malloc, Lam> {
public:
    Alloc2Malloc(PassMan& man)
        : RWPass(man, "alloc2malloc") {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem
