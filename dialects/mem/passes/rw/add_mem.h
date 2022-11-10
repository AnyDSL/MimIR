#ifndef THORIN_PHASE_RW_ADD_MEM_H
#define THORIN_PHASE_RW_ADD_MEM_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>
#include <thorin/phase/phase.h>

namespace thorin::mem {

class AddMem : public RWPhase {
public:
    AddMem(World& world)
        : RWPhase(world, "add_mem") {}

    const Def* rewrite_structural(const Def*) override;

    static PassTag* ID();

private:
    Def2Def rewritten;
};

} // namespace thorin::mem

#endif
