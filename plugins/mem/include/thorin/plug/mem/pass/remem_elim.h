#pragma once

#include <thorin/pass/pass.h>

namespace thorin::plug::mem {

class RememElim : public RWPass<RememElim, Lam> {
public:
    RememElim(PassMan& man)
        : RWPass(man, "remem_elim") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::plug::mem
