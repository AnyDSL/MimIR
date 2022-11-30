#pragma once

#include "thorin/analyses/schedule.h"
#include "thorin/phase/phase.h"

namespace thorin::mem {

/// This phase adds mems to all lambdas and continuations.
/// It's primarily to be used as preparation for other phases
/// that rely on all continuations having a mem.
class AddMem : public ScopePhase {
public:
    AddMem(World& world)
        : ScopePhase(world, "add_mem", true) {
        dirty_ = true;
    }

    void visit(const Scope&) override;

private:
    const Def* add_mem_to_lams(Lam*, const Def*);
    const Def* rewrite_type(const Def*);
    const Def* rewrite_pi(const Pi*);
    const Def* mem_for_lam(Lam*) const;

    Scheduler sched_;
    Def2Def val2mem_;
    Def2Def mem_rewritten_;
};

class AddMemWrapper : public RWPass<AddMemWrapper, Lam> {
public:
    AddMemWrapper(PassMan& man)
        : RWPass(man, "add_mem") {}

    void prepare() override { mem::AddMem(world()).run(); }
};

} // namespace thorin::mem
