#pragma once

#include "thorin/analyses/schedule.h"
#include "thorin/phase/phase.h"

namespace thorin::mem {

class AddMem : public RWPhase {
public:
    AddMem(World& world)
        : RWPhase(world, "add_mem") {}

    void start() override;
    const Def* rewrite(const Def*) override;

private:
    const Def* add_mem_to_lams(Lam*, const Def*);
    Scheduler& sched() { return sched_.back(); }

    std::vector<Scheduler> sched_;
    Lam* curr_external_;
    Def2Def val2mem_;
    Def2Def mem_rewritten_;
};

} // namespace thorin::mem
