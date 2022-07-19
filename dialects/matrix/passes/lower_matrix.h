#ifndef THORIN_PASS_RW_LOWER_MATRIX_H
#define THORIN_PASS_RW_LOWER_MATRIX_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::matrix {

/// Resolved by normalizer:
/// - shape
/// - transpose
/// Rewrites into loop:
/// - product
/// - map
/// - zipWith
/// - fold
/// - id
/// - constMat
/// Left for final phase:
/// - Mat
/// - read
/// - insert

/// Lowers the for axiom to actual control flow in CPS style
/// Requires CopyProp to cleanup afterwards.
///
/// lowers all high level matrix operations to low level matrix interactions in loops
/// for instance, `map` becomes a loop with read and writes
///
/// matrix operations such as map are in direct calling position
/// but need to be translated to CPS
/// We use the direct style dialect plugin to do this
class LowerMatrix : public RWPass<Lam> {
public:
    LowerMatrix(PassMan& man)
        : RWPass(man, "lower_matrix") {}

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
