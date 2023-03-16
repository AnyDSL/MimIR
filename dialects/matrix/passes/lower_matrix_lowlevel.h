#ifndef THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H
#define THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>
#include <thorin/phase/phase.h>

namespace thorin::matrix {

class LowerMatrixLowLevel : public RWPhase {
public:
    LowerMatrixLowLevel(World& world)
        : RWPhase(world, "lower_matrix_lowlevel") {}

    const Def* rewrite_structural(const Def*) override;

private:
    Def2Def rewritten;
};

} // namespace thorin::matrix

#endif
