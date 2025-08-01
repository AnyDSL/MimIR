#pragma once

#include <mim/schedule.h>

#include <mim/phase/phase.h>

namespace mim::plug::buffrize {

class Bufferize : public NestPhase<Lam> {
public:
    Bufferize(World& world)
        : NestPhase(world, "buffrize", true, true) {}

    void visit(const Nest&) override;

private:
    const Def* visit_def(const Def *);

    const Def* visit_insert(Lam* place, const Insert* insert);
    const Def* visit_extract(Lam* place, const Extract* extract);

    const Def* active_mem(Lam* place);
    void add_mem(const Lam* place, const Def* mem);
    
    Scheduler sched_;
    // Memoization & Association for rewritten defs.
    Def2Def rewritten_;
    // const Def *mem_;
    DefMap<DefVec> lam2mem_;
};

} // namespace mim::plug::mem
