#ifndef THORIN_PASS_RW_LOWER_MATRIX_H
#define THORIN_PASS_RW_LOWER_MATRIX_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::matrix {

/// Lowers the for axiom to actual control flow in CPS style
/// Requires CopyProp to cleanup afterwards.
class LowerMatrix : public RWPass<Lam> {
public:
    LowerMatrix(PassMan& man)
        : RWPass(man, "lower_matrix") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
};

} // namespace thorin::matrix

#endif
