#include "dialects/mem/passes/fp/copy_prop.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* CopyProp::rewrite(const Def* def) {
    auto [app, var_lam] = isa_apped_nom_lam(def);
    if (!isa_workable(var_lam) || (bb_only_ && var_lam->is_returning())) return def;

    auto n = app->num_args();
    if (n == 0) return app;

    auto [it, _] = lam2info_.emplace(var_lam, std::tuple(Lattices(n), (Lam*)nullptr, DefArray(n)));
    auto& [lattice, prop_lam, old_args] = it->second;

    if (mem::mem_var(var_lam)) lattice[0] = Lattice::Keep;
    if (std::ranges::all_of(lattice, [](auto l) { return l == Lattice::Keep; })) return app;

    DefVec new_args, new_doms, appxy_ops = {var_lam};
    auto& args = data(var_lam);
    args.resize(n);

    for (size_t i = 0; i != n; ++i) {
        switch (lattice[i]) {
            case Lattice::Dead: break;
            case Lattice::Prop:
                if (app->arg(i)->has_dep(Dep::Proxy)) {
                    world().DLOG("found proxy within app: {}@{} - wait till proxy is gone", var_lam, app);
                    return app;
                } else if (args[i] == nullptr) {
                    args[i] = app->arg(i);
                } else if (args[i] != app->arg(i)) {
                    appxy_ops.emplace_back(world().lit_nat(i));
                } else {
                    assert(args[i] == app->arg(i));
                }
                break;
            case Lattice::Keep:
                new_doms.emplace_back(var_lam->var(i)->type());
                new_args.emplace_back(app->arg(i));
                break;
            default: unreachable();
        }
    }

    world().DLOG("app->args(): {, }", app->args());
    world().DLOG("args: {, }", args);
    world().DLOG("new_args: {, }", new_args);

    if (appxy_ops.size() > 1) {
        auto appxy = proxy(app->type(), appxy_ops, Appxy);
        world().DLOG("appxy: '{}': {, }", appxy, appxy_ops);
        return appxy;
    }

    assert(new_args.size() < n);
    if (prop_lam == nullptr || old_args != args) {
        old_args      = args;
        auto prop_dom = world().sigma(new_doms);
        auto new_pi   = world().pi(prop_dom, var_lam->codom());
        prop_lam      = var_lam->stub(world(), new_pi);

        world().DLOG("new prop_lam: {}", prop_lam);
        if (beta_red_) beta_red_->keep(prop_lam);
        if (eta_exp_) eta_exp_->new2old(prop_lam, var_lam);

        size_t j = 0;
        DefArray new_vars(n, [&, prop_lam = prop_lam](size_t i) -> const Def* {
            switch (lattice[i]) {
                case Lattice::Dead: return proxy(var_lam->var(i)->type(), {var_lam, world().lit_nat(i)}, Varxy);
                case Lattice::Prop: return args[i];
                case Lattice::Keep: return prop_lam->var(j++);
                default: unreachable();
            }
        });

        prop_lam->set(var_lam->reduce(world().tuple(new_vars)));
    }

    world().DLOG("var_lam => prop_lam: {}: {} => {}: {}", var_lam, var_lam->dom(), prop_lam, prop_lam->dom());
    auto res = app->world().app(prop_lam, new_args);

    // Don't optimize again. Also, keep this line here at the very bottom as this invalidates all references.
    Lam* key = prop_lam; // prop_lam is a Lam*& which might get invalidated by the very insertion happening next.
    lam2info_.insert_or_assign(key, std::tuple(Lattices(new_doms.size(), Lattice::Keep), (Lam*)nullptr, DefArray()));
    return res;
}

undo_t CopyProp::analyze(const Proxy* proxy) {
    world().DLOG("found proxy: {}", proxy);
    auto var_lam                        = proxy->op(0)->as_nom<Lam>();
    auto& [lattice, prop_lam, old_args] = lam2info_[var_lam];

    if (proxy->tag() == Varxy) {
        auto i = as_lit(proxy->op(1));
        if (auto& l = lattice[i]; l == Lattice::Dead) {
            l = Lattice::Prop;
            world().DLOG("Dead -> Prop: @{}#{}", var_lam, i);
            return undo_visit(var_lam);
        }
    } else {
        assert(proxy->tag() == Appxy);
        auto ops = proxy->ops();
        for (auto op : ops.skip_front()) {
            auto i = as_lit(op);
            if (auto& l = lattice[i]; l != Lattice::Keep) {
                l = Lattice::Keep;
                world().DLOG("Prop -> Keep: @{}#{}", var_lam, i);
            }
        }

        return undo_visit(var_lam);
    }

    return No_Undo;
}

} // namespace thorin::mem
