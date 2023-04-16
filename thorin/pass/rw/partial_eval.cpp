#include "thorin/pass/rw/partial_eval.h"

#include "thorin/rewrite.h"

namespace thorin {

Ref PartialEval::rewrite(Ref def) {
    if (auto [app, lam] = isa_apped_mut_lam(def); lam && lam->is_set()) {
        if (lam->filter() == world().lit_ff()) return def; // optimize this common case

        auto [filter, body] = lam->reduce(app->arg()).to_array<2>();
        if (auto f = Lit::isa<bool>(filter); f && *f) {
            world().DLOG("PE {} within {}", lam, curr_mut());
            return body;
        }
    }

    return def;
}

} // namespace thorin
