#pragma once

#include <mim/phase.h>
#include <mim/driver.h>
#include <mim/schedule.h>

namespace mim::plug::mem {

/// This phase adds mems to all lambdas and continuations.
/// It's primarily to be used as preparation for other phases
/// that rely on all continuations having a mem.
class AddMem : public NestPhase<Lam> {
public:
    AddMem(World& world, flags_t annex)
        : NestPhase(world, annex, true) {}
    AddMem* recreate() final { return driver().stage<AddMem>(world(), annex()); }

    void visit(const Nest&) override;

private:
    const Def* add_mem_to_lams(Lam*, const Def*);
    const Def* rewrite_type(const Def*);
    const Def* rewrite_pi(const Pi*);
    /// Return the most recent memory for the given lambda.
    const Def* mem_for_lam(Lam*) const;

    Scheduler sched_;
    // Stores the most recent memory for a lambda.
    Def2Def val2mem_;
    // Memoization & Association for rewritten defs.
    Def2Def mem_rewritten_;
};

} // namespace mim::plug::mem
