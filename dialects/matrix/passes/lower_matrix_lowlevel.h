#ifndef THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H
#define THORIN_PASS_RW_LOWER_MATRIX_LOWLEVEL_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::matrix {

class LowerMatrixLowLevel : public RWPass<LowerMatrixLowLevel, Lam> {
public:
    LowerMatrixLowLevel(PassMan& man)
        : RWPass(man, "lower_matrix_lowlevel") {}

    /// custom rewrite function
    /// memoized version of rewrite_
    const Def* rewrite(const Def*) override;
    const Def* rewrite_(const Def*);

    static PassTag* ID();

private:
    Def2Def rewritten;
};

} // namespace thorin::matrix

#endif
