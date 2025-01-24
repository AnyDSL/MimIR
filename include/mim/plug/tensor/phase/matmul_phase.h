#pragma once

#include <mim/phase/phase.h>

namespace mim::plug::tensor {

class MatmulPhase : public RWPhase {
public:
    MatmulPhase(World& world)
        : RWPhase(world, "matmul_phase") {}

    Ref rewrite_imm(Ref) override;
};

} // namespace mim::plug::tensor
