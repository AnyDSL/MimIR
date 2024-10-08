#pragma once

#include <mim/pass/pass.h>

namespace mim::plug::mem {

class RememElim : public RWPass<RememElim, Lam> {
public:
    RememElim(PassMan& man)
        : RWPass(man, "remem_elim") {}

    Ref rewrite(Ref) override;
};

} // namespace mim::plug::mem
