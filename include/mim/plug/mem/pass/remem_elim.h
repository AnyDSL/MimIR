#pragma once

#include <mim/pass.h>

namespace mim::plug::mem {

class RememElim : public RWPass<RememElim, Lam> {
public:
    RememElim(PassMan& man)
        : RWPass(man, "remem_elim") {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::mem
