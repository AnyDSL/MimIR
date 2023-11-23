#pragma once

#include <map>
#include <tuple>

#include "thorin/check.h"

#include "thorin/pass/pass.h"

namespace thorin::clos {

class BranchClosElim : public RWPass<BranchClosElim, Lam> {
public:
    BranchClosElim(PassMan& man)
        : RWPass(man, "branch_clos_elim")
        , branch2dropped_() {}

    Ref rewrite(Ref) override;

private:
    DefMap<Lam*> branch2dropped_;
};

}; // namespace thorin::clos
