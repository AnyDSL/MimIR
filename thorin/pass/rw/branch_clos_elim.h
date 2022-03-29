#ifndef THORIN_UNBOX_CLOSURE_H
#define THORIN_UNBOX_CLOSURE_H

#include <map>
#include <tuple>

#include "thorin/check.h"

#include "thorin/pass/pass.h"

namespace thorin {

class BranchClosElim : public RWPass<Lam> {
public:
    BranchClosElim(PassMan& man)
        : RWPass<Lam>(man, "unbox_closures")
        , branch2dropped_() {}

    const Def* rewrite(const Def*) override;

private:
    DefMap<Lam*> branch2dropped_;
};

}; // namespace thorin

#endif
