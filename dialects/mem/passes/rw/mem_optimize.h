#pragma once

#include "thorin/pass/pass.h"

namespace thorin::mem {

class MemOptimize : public RWPass<MemOptimize, Lam> {
public:
    MemOptimize(PassMan& man)
        : RWPass(man, "mem_optimize") {}

    const Def* rewrite(const Def*) override;

    const Def* optimize_load(const Def* mem, const Def* ptr);
    const Def* optimize_store(const Def* mem, const Def* ptr);
};

} // namespace thorin::mem
