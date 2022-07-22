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

/// pseudo code to lower mapReduce:
/// * out indices = (0,1,2, ..., n)
/// * bounds in S
/// * we assume that certain paramters are constant and statically known
///   to avoid inline-metaprogramming like multiiter
///   e.g. the number of matrizes, the dimensions, the indices
/// ```
/// // iterate over out indices
/// output = init_matrix (n,S,T)
/// for i_0 in [0, S#0)
///   ...
///     for i_{n-1} in [0, S#(n-1))
///       s = zero
///       // iterate over non-out indices
///       for j in [0, SI#(...)]:
///         // indices depend on the specified access
///         // input#k#0
///         e_0 = read (input#0#1, (i_1, i_0))
///         ...
///         e_(m-1) = read (input#(m-1)#1, (i_2, j))
///
///         s = add(s, mul (e_0, ..., e_(m-1)) )
///       write (output, (i_0, ..., i_{n-1}), s)
/// ```
/// TODO: identify patterns and emit specialized operations like matrix product (blas)
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