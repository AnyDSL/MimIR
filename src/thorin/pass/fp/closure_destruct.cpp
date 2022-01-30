
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
        if (keep_.contains(clam) || c.env_type() == w.sigma())
            return pack_closure(c.env(), c.fnc_as_lam(),  c.type()); // Get rid of the annotations
        auto& old_env = data(clam);
        if (old_env && old_env != c.env())
            return proxy(c.type(), {clam, old_env, c.env()});
        old_env = c.env();
        auto& dropped_lam = clos2dropped_[clam];
        if (!dropped_lam) {
            dropped_lam = c.fnc_as_lam()->stub(w, ctype_to_pi(c.type(), w.sigma()), c.fnc_as_lam()->dbg());
            auto new_vars = DefArray(dropped_lam->num_doms(), [&](auto i) {
                return (i == CLOSURE_ENV_PARAM) ? c.env() : dropped_lam->var(i); 
            });
            dropped_lam->set(c.fnc_as_lam()->apply(w.tuple(new_vars)));
            dropped2clos_[dropped_lam] = clam;
            w.DLOG("dropped ({}, {}) => {}", c.env(), c.fnc_as_lam(), dropped_lam);
        }
        return pack_closure(w.tuple(), dropped_lam, c.type());
    }

    return def;
}

undo_t ClosureDestruct::analyze(const Proxy* proxy) {
    auto [clam, old_env, new_env] = proxy->ops<3>();
    world().DLOG("possible join point {}: env {} != {}", clam, old_env, new_env);
    keep_.emplace(clam->as_nom<Lam>());
    return undo_visit(clam->as_nom<Lam>());
}

undo_t Closure2SSI::analyze(const Def* def) {
    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        auto clos_branch = app->callee()->isa<Extract>();  // closur call
        if (!clos_branch || !isa_ctype(clos_branch->type()))
            return No_Undo;
        clos_branch = clos_branch->isa<Extract>(); // branches
        if (!clos_branch || !clos_branch->tuple()->isa<Tuple>())
            return No_Undo;
        auto undo = No_Undo;
        for (auto op: clos_branch->tuple()->ops()) {
            if (auto c = isa_closure_lit(op); c && c.fnc_as_lam()) {
                auto old_lam = dropped2clos_[c.fnc_as_lam()];
                if (keep_.contains(old_lam)) {
                    world().DLOG("dropped closure in branch: {}", dropped2clos_[c.fnc_as_lam()]);
                    undo = std::min(undo, undo_visit(old_lam));
                }
            }
        }
        return undo;
    }

    return No_Undo;

}


} // namespace thorin
