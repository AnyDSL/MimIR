
#include "thorin/transform/closure_conv.h"
#include "thorin/pass/fp/closure_destruct.h"

namespace thorin {


const Def* ClosureDestruct::rewrite(const Def* def) {
    auto& w = world();

    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        if (auto [callee, _] = ca_isa_mark(app->callee()); callee) {
            w.DLOG("removed mark from callee {} in {}", callee, app);
            return w.app(callee, app->arg(), app->dbg());
        }
    }

    if (auto c = isa_closure_lit(def); c && c.is_basicblock()) {
        auto clam = c.fnc_as_lam();
        if (!clam)
            return c;
        if (c.env_type() == w.sigma())
            return pack_closure(c.env(), clam,  c.type()); // Get rid of the annotations
        auto& dropped_lam = clos2dropped_[w.tuple({clam, c.env()})];
        if (!dropped_lam) {
            dropped_lam = c.fnc_as_lam()->stub(w, ctype_to_pi(c.type(), w.sigma()), clam->dbg());
            auto new_vars = DefArray(dropped_lam->num_doms(), [&](auto i) {
                return (i == CLOSURE_ENV_PARAM) ? c.env() : dropped_lam->var(i); 
            });
            dropped_lam->set(c.fnc_as_lam()->apply(w.tuple(new_vars)));
            w.DLOG("dropped ({}, {}) => {}", c.env(), c.fnc_as_lam(), dropped_lam);
        }
        return pack_closure(w.tuple(), dropped_lam, c.type());
    }

    return def;
}

} // namespace thorin
