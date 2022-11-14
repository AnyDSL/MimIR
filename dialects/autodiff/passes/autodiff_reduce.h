#pragma once

#include "thorin/pass/pass.h"

namespace thorin::autodiff {

class AutodiffReduce : public RWPass<AutodiffReduce, Lam> {
public:
    AutodiffReduce(PassMan& man)
        : RWPass(man, "autodiff_reduce") {}

    const Def* rewrite(const Def*) override;
    Lam* reduce(Lam* lam);
    const Def* reduce(const Def* lam, const Def* ret);
    const Def* wrap(const Def* def);

    DefSet visited;
    Def2Def return_wrappers;
};

} // namespace thorin::autodiff
