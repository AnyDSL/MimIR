#ifndef THORIN_CLOS_PASS_FP_LOWER_TYPED_CLOS_PREP_H
#define THORIN_CLOS_PASS_FP_LOWER_TYPED_CLOS_PREP_H

#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/pass.h"

#include "dialects/clos/clos_conv.h"

namespace thorin::clos {

class LowerTypedClosPrep : public FPPass<LowerTypedClosPrep, Lam> {
public:
    LowerTypedClosPrep(PassMan& man)
        : FPPass<LowerTypedClosPrep, Lam>(man, "closure_analysis")
        , esc_() {}

    using Data = int; // Dummy

private:
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Def*) override;

    bool is_esc(const Def* def) {
        if (auto [_, lam] = ca_isa_var<Lam>(def); lam && !lam->is_set()) return true;
        return esc_.contains(def);
    }
    undo_t set_esc(const Def*);

    DefSet esc_;
};

} // namespace thorin::clos

#endif
