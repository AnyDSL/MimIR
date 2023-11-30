#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class EtaExp;

/// Perform Scalarization (= Argument simplification), i.e.:
/// ```
/// f := λ (x_1:[T_1, T_2], .., x_n:T_n).E`
/// ```
/// will be transformed to
/// ```
/// f' := λ (y_1:T_1, y_2:T2, .. y_n:T_n).E[x_1 \ (y_1, y2); ..; x_n \ y_n]
/// ```
/// if `f` appears in callee position only (see @p EtaExp).
/// It will not flatten mutable @p Sigma%s or @p Arr%ays.
class Scalerize : public RWPass<Scalerize, Lam> {
public:
    Scalerize(PassMan& man, EtaExp* eta_exp)
        : RWPass(man, "scalerize")
        , eta_exp_(eta_exp) {}

    Ref rewrite(Ref) override;

private:
    bool should_expand(Lam* lam);
    Lam* make_scalar(Ref def);

    EtaExp* eta_exp_;
    Lam2Lam tup2sca_;
};

} // namespace thorin
