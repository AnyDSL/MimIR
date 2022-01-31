
#include "thorin/transform/closure_conv.h"
#include "thorin/pass/fp/closure_destruct.h"

namespace thorin {

const Def* Closure2SSI::try_drop(const Def* def) {
    auto& w = world();
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

const Def* Closure2SSI::rewrite(const Def* def) {
    auto& w = world();

    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        const Def* new_callee;
        if (auto [callee, _] = ca_isa_mark(app->callee()); callee) {
            w.DLOG("removed mark from callee {} in {}", callee, app);
            new_callee = callee;
        } else {
            new_callee = app->callee();
        }

        auto new_args = DefArray(app->num_args(), [&](auto i) { return try_drop(app->arg(i)); });
        return w.app(new_callee, new_args, app->dbg());
    }

    return def;
}

undo_t Closure2SSI::analyze(const Proxy* proxy) {
    auto [clam, old_env, new_env] = proxy->ops<3>();
    world().DLOG("possible join point {}: env {} != {}", clam, old_env, new_env);
    keep_.emplace(clam->as_nom<Lam>());
    return undo_visit(clam->as_nom<Lam>());
}

const Def* ClosureDestruct::rewrite(const Def* def) {
    auto& w = world();
    def = Closure2SSI::rewrite(def);
    if (auto app = def->isa<App>(); app && app->callee_type()->is_cn()) {
        auto closure_fnc = app->callee()->isa<Extract>(); // closure call, get closure
        if (!closure_fnc)
            return def;
        auto cbranch = closure_fnc->tuple()->isa<Extract>(); // closure is a branch
        if (!cbranch || !isa_ctype(cbranch->type()) || !cbranch->tuple()->isa<Tuple>())
            return def;
        auto tuple = cbranch->tuple();
        auto new_tuple = w.tuple(DefArray(tuple->num_ops(), [&](auto i) { return try_drop(tuple->op(i)); }));
        auto new_branch = w.extract(new_tuple, cbranch->index(), cbranch->dbg());
        if (new_branch != cbranch) {
            w.DLOG("^ dropped branch");
            return apply_closure(new_branch, closure_remove_env(app->arg()));
        }
    }
    return def;
}



} // namespace thorin
