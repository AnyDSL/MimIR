#ifndef THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H
#define THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>
#include <thorin/phase/phase.h>

namespace thorin::matrix {

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

    Ref rewrite_structural(Ref) override;

private:
    Def2Def rewritten;
};

} // namespace thorin::matrix

#endif
