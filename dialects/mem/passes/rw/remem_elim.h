#ifndef THORIN_PASS_RW_REMEM_ELIM_H
#define THORIN_PASS_RW_REMEM_ELIM_H

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

#endif
