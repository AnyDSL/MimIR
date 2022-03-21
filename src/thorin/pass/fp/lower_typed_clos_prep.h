
#ifndef THORIN_PASS_ASSIGN_RET_H
#define THORIN_PASS_ASSIGN_RET_H

#include "thorin/pass/pass.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/transform/clos_conv.h"

namespace thorin {

class LowerTypedClosPrep : public FPPass<LowerTypedClosPrep, Lam> {
public:
    LowerTypedClosPrep(PassMan& man)
        : FPPass<LowerTypedClosPrep, Lam>(man, "closure_analysis")
        , escaping_() {}

    using Data = int; // Dummy

private:

    const Def* rewrite(const Def*) override;
    undo_t analyze(const Def*) override;

    bool is_escaping(const Def* def) { 
        if (auto [_, lam] = ca_isa_var<Lam>(def); lam && !lam->is_set())
            return true;
        return escaping_.contains(def); 
    }
    undo_t set_escaping(const Def*);

    DefSet escaping_;
};

}

#endif
