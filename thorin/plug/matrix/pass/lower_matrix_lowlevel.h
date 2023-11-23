#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>
#include <thorin/phase/phase.h>

namespace thorin::plug::matrix {

/// In this phase, we lower all matrix operations and types to the low-level representation using pointers.
/// The matrix type is replaced by a pointer to n nested arrays.
/// - `init` is replaced with `alloc`
/// - `read` becomes `lea+load`
/// - `insert` becomes `lea+store`
/// - `constMat` becomes `alloc+pack+store`

class LowerMatrixLowLevel : public RWPhase {
public:
    LowerMatrixLowLevel(World& world)
        : RWPhase(world, "lower_matrix_lowlevel") {}

    Ref rewrite_imm(Ref) override;

private:
    Def2Def rewritten;
};

} // namespace thorin::plug::matrix
