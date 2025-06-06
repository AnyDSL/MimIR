#pragma once

#include <mim/pass/eta_exp.h>
#include <mim/pass/pass.h>

#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

class LowerTypedClosPrep : public FPPass<LowerTypedClosPrep, Lam> {
public:
    LowerTypedClosPrep(PassMan& man)
        : FPPass<LowerTypedClosPrep, Lam>(man, "lower_typed_clos_prep") {}

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

} // namespace mim::plug::clos
