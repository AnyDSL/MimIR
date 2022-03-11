#include "thorin/pass/fp/dce.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"

namespace thorin {

const Def* DCE::rewrite(const Def* def) {
    if (auto [app, var_lam] = isa_apped_nom_lam(def); isa_workable(var_lam)) return var2dead(app, var_lam);
    return def;
}

const Def* DCE::var2dead(const App* app, Lam* var_lam) {
    if (var_lam->num_vars() == 0 || keep_.contains(var_lam)) return app;

    DefVec new_args;
    DefVec types;
    BitSet live;

    for (size_t i = 0, e = app->num_args(); i != e; ++i) {
        if (keep_.contains(var_lam->var(i))) {
            types.emplace_back(var_lam->var(i)->type());
            new_args.emplace_back(app->arg(i));
            live.set(i);
        }
    }

    if (new_args.size() == var_lam->num_vars()) {
        auto&& [_, ins] = keep_.emplace(var_lam);
        assert(ins);
        return app;
    }

    world().DLOG("app->args(): {, }", app->args());
    world().DLOG("new_args: {, }", new_args);

    auto&& [dead_lam, old_live] = var2dead_[var_lam];
    if (dead_lam == nullptr || old_live != live) {
        old_live      = live;
        auto dead_dom = world().sigma(types);
        auto new_type = world().pi(dead_dom, var_lam->codom());
        dead_lam      = var_lam->stub(world(), new_type, var_lam->dbg());
        beta_red_->keep(dead_lam);
        eta_exp_->new2old(dead_lam, var_lam);
        keep_.emplace(dead_lam); // don't try to dce again
        world().DLOG("var_lam => dead_lam: {}: {} => {}: {}", var_lam, var_lam->type()->dom(), dead_lam, dead_dom);

        size_t j = 0;
        DefArray new_vars(app->num_args(), [&, dead_lam = dead_lam](size_t i) {
            auto v = var_lam->var(i);
            return keep_.contains(v) ? dead_lam->var(j++) : proxy(v->type(), {var_lam, v}, 0);
        });
        dead_lam->set(var_lam->reduce(world().tuple(new_vars)));
    } else {
        world().DLOG("reuse var_lam => dead_lam: {}: {} => {}: {}", var_lam, var_lam->type()->dom(), dead_lam,
                     dead_lam->type()->dom());
    }

    return app->world().app(dead_lam, new_args, app->dbg());
}

undo_t DCE::analyze(const Proxy* proxy) {
    auto var_lam = proxy->op(0)->as_nom<Lam>();
    auto v       = proxy->op(1);
    world().DLOG("found proxy: {}", v);

    if (keep_.emplace(v).second) return undo_visit(var_lam);

    return No_Undo;
}

} // namespace thorin
