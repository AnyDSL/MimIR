#pragma once

#include <map>
#include <tuple>

#include "thorin/check.h"

#include "thorin/pass/pass.h"

namespace thorin::clos {

class BranchClosElim : public RWPass<BranchClosElim, Lam> {
public:
    BranchClosElim(PassMan& man)
        : RWPass(man, "unbox_closures")
        , branch2dropped_() {}

    const Def* rewrite(const Def*) override;

private:
    DefMap<Lam*> branch2dropped_;
};

}; // namespace thorin::clos
