#pragma once

#include <map>
#include <tuple>

#include <mim/pass/pass.h>

namespace mim::plug::clos {

class BranchClosElim : public RWPass<BranchClosElim, Lam> {
public:
    BranchClosElim(PassMan& man)
        : RWPass(man, "branch_clos_elim")
        , branch2dropped_() {}

    const Def* rewrite(const Def*) override;

private:
    DefMap<Lam*> branch2dropped_;
};

}; // namespace mim::plug::clos
