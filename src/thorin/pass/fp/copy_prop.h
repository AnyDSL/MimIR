#ifndef THORIN_PASS_FP_COPY_PROP_H
#define THORIN_PASS_FP_COPY_PROP_H

#include "thorin/pass/pass.h"

namespace thorin {

<<<<<<< HEAD
/// This @p FPPass is similar to sparse conditional constant propagation (SCCP) but also propagates arbitrary values through @p Var%s.
=======
class BetaRed;
class EtaExp;

/// This @p FPPass is similar to sparse conditional constant propagation (SCCP).
>>>>>>> main/t2
/// However, this optmization also works on all @p Lam%s alike and does not only consider basic blocks as opposed to traditional SCCP.
/// What is more, this optimization will also propagate arbitrary @p Def%s and not only constants. <br>
class CopyProp : public FPPass<CopyProp, Lam> {
public:
<<<<<<< HEAD
    CopyProp(PassMan& man)
        : FPPass(man, "copy_prop")
=======
    CopyProp(PassMan& man, BetaRed* beta_red, EtaExp* eta_exp)
        : FPPass(man, "copy_prop")
        , beta_red_(beta_red)
        , eta_exp_(eta_exp)
>>>>>>> main/t2
    {}

    using Args = std::vector<const Def*>;
    using Data = LamMap<Args>;

private:
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;
<<<<<<< HEAD
    undo_t analyze(const Def*) override;

    Lam2Lam var2prop_;
=======
    //@}

    const Def* var2prop(const App*, Lam*);

    BetaRed* beta_red_;
    EtaExp* eta_exp_;
    LamMap<std::pair<Lam*, DefVec>> var2prop_;
>>>>>>> main/t2
    DefSet keep_;
};

}

#endif
