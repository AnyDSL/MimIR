#ifndef THORIN_PASS_RW_LOWER_MATRIX_HIGHLEVEL_H
#define THORIN_PASS_RW_LOWER_MATRIX_HIGHLEVEL_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::matrix {

/// Resolves lowering of high level operations into medium/other high-level operations.
/// Some of these transformations could be done as normalizer.
/// We rewrite matrix operations like sum, transpose, and product into `mapReduce` operations.
/// The corresponding `mapReduce` operation is looked up as `internal_mapRed_matrix_[name]`.

class LowerMatrixHighLevelMapRed : public RWPass<LowerMatrixHighLevelMapRed, Lam> {
public:
    LowerMatrixHighLevelMapRed(PassMan& man)
        : RWPass(man, "lower_matrix_highlevel") {}

    /// custom rewrite function
    /// memoized version of rewrite_
    const Def* rewrite(const Def*) override;
    const Def* rewrite_(const Def*);

private:
    Def2Def rewritten;
};

} // namespace thorin::matrix

#endif
