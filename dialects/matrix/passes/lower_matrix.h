#ifndef THORIN_PASS_RW_LOWER_MATRIX_H
#define THORIN_PASS_RW_LOWER_MATRIX_H

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::matrix {

/// Lowers the for axiom to actual control flow in CPS style
/// Requires CopyProp to cleanup afterwards.
///
/// lowers all high level matrix operations to low level matrix interactions in loops
/// for instance, `map` becomes a loop with read and writes
///
/// matrix operations such as map are in direct calling position
/// but need to be translated to CPS
/// Therefore, a custom traversal order is necessary
/// as the bodys of the functions are replaced and the original body
/// is simultaneously changed
/// 
/// ````
/// f(...): 
///   x = map ...
///   C[x]
/// ````
/// becomes
/// ````
/// f(...): 
///   mapping_call args, g // g as continuation
///
/// g(result):
///   C[result]
/// ````
class LowerMatrix : public RWPass<Lam> {
public:
    LowerMatrix(PassMan& man)
        : RWPass(man, "lower_matrix") {}

    /// custom rewrite function
    const Def* rewrite_(const Def*);

    /// main entry point for this pass
    /// rewrites curr_nom()
    void enter() override;

    static PassTag* ID();

private:
    Def2Def rewritten_;
    Lam* currentLambda;
};

} // namespace thorin::matrix

#endif
