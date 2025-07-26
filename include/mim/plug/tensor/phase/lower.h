#pragma once

#include <mim/phase/phase.h>

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

class Lower : public RWPhase {
public:
    Lower(World& world)
        : RWPhase(world, "tensor_lower") {}

private:
    const Def* rewrite_imm(const Def*) override;
    const Def* lower_map_reduce(const App*);
};

} // namespace mim::plug::tensor
