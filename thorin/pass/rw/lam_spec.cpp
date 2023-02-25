#include "thorin/pass/rw/lam_spec.h"

#include "thorin/analyses/scope.h"
#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/util/container.h"

// TODO This is supposed to recreate what lower2cff did, but we should really consider another strategy and nuke this.

namespace thorin {

static bool is_top_level(LamMap<bool>& top, Lam* lam) {
    if (lam->is_external()) return true;
    if (auto [i, ins] = top.emplace(lam, true); !ins) return i->second;

    Scope scope(lam);
    if (!scope.free_vars().empty()) return top[lam] = false;

    for (auto nom : scope.free_noms()) {
        if (auto inner = nom->isa<Lam>()) {
            if (!is_top_level(top, inner)) return top[lam] = false;
        }
    }

    return true;
}

static bool is_top_level(Lam* lam) {
    LamMap<bool> top;
    return is_top_level(top, lam);
}

const Def* LamSpec::rewrite(const Def* def) {
    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;

    auto [app, old_lam] = isa_apped_nom_lam(def);
    if (!isa_workable(old_lam)) return def;

    DefVec new_doms, new_vars, new_args;
    auto skip     = old_lam->ret_var() ? (is_top_level(old_lam) ? 1 : 0) : 0;
    auto old_doms = old_lam->doms();

    for (auto dom : old_doms.skip_back(skip)) {
        if (!dom->isa<Pi>()) new_doms.emplace_back(dom);
    }

    if (skip) new_doms.emplace_back(old_lam->doms().back());
    if (new_doms.size() == old_lam->num_doms()) return def;

    auto new_pi  = world().cn(world().sigma(new_doms));
    auto new_lam = old_lam->stub(world(), new_pi);

    for (size_t arg_i = 0, var_i = 0, n = app->num_args() - skip; arg_i != n; ++arg_i) {
        auto arg = app->arg(arg_i);
        if (old_lam->dom(arg_i)->isa<Pi>()) {
            new_vars.emplace_back(arg);
        } else {
            new_vars.emplace_back(new_lam->var(var_i++));
            new_args.emplace_back(arg);
        }
    }

    if (skip) {
        new_vars.emplace_back(new_lam->vars().back());
        new_args.emplace_back(app->args().back());
    }

    new_lam->set(old_lam->reduce(world().tuple(new_vars)));
    world().DLOG("{} -> {}: {} -> {})", old_lam, new_lam, old_lam->dom(), new_lam->dom());

    return old2new_[def] = world().app(new_lam, new_args);
}

} // namespace thorin
