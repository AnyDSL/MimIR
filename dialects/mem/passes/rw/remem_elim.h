#pragma once

#include "thorin/pass/pass.h"

namespace thorin::mem {

class RememElim : public RWPass<Lam> {
public:
    RememElim(PassMan& man)
        : RWPass(man, "remem_elim") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();
};

} // namespace thorin::mem
