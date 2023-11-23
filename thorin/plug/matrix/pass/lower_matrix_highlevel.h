#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::plug::matrix {

/// Resolves lowering of high level operations into medium/other high-level operations.
/// Some of these transformations could be done as normalizer.
/// We rewrite matrix operations like sum, transpose, and product into `map_reduce` operations.
/// The corresponding `map_reduce` operation is looked up as `internal_mapRed_matrix_[name]`.

class LowerMatrixHighLevelMapRed : public RWPass<LowerMatrixHighLevelMapRed, Lam> {
public:
    LowerMatrixHighLevelMapRed(PassMan& man)
        : RWPass(man, "lower_matrix_highlevel") {}

    /// custom rewrite function
    /// memoized version of rewrite_
    Ref rewrite(Ref) override;
    Ref rewrite_(Ref);

private:
    Def2Def rewritten;
};

} // namespace thorin::plug::matrix
