#pragma once

#include <mim/phase.h>

namespace mim::plug::tensor::phase {

class Lower : public RWPhase {
public:
    Lower(World& world, flags_t annex)
        : RWPhase(world, annex) {}

private:
    const Def* rewrite_imm_App(const App*) final;

    const Def* lower_get(const App*);
    const Def* lower_set(const App*);
};

} // namespace mim::plug::tensor
