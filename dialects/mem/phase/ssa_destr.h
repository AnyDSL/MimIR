#pragma once

#include "thorin/phase/phase.h"

namespace thorin::mem {

class SSADestr : public RWPhase {
public:
    SSADestr(World& world)
        : RWPhase(world, "ssa_destr") {}

    void start() override;
};

} // namespace thorin::mem
