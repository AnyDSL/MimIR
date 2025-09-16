#pragma once

#include <mim/phase.h>

namespace mim::plug::tensor {

class Lower : public RWPhase {
public:
    Lower(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    const Def* rewrite_imm_App(const App*) final;
};

} // namespace mim::plug::tensor
