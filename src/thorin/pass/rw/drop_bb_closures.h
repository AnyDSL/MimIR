#ifndef THORIN_CLOSURE_DESTRUCT_H
#define THORIN_CLOSURE_DESTRUCT_H

#include <set>
#include <functional>

#include "thorin/pass/pass.h"
#include "thorin/pass/fp/eta_exp.h"

namespace thorin {

/// Remove closures for @p Lam's that can be compiled to basic blocks, i.e.
/// are annotated with @p :CA_ret or @p :CA_jmp. 

class DropBBClosures : public RWPass<Lam> {
public:
    DropBBClosures(PassMan& man) 
        : RWPass(man, "closure_destruct")
    {}

    const Def* rewrite(const Def*) override;

private:

    DefMap<Lam*> clos2dropped_;
};

}

#endif
