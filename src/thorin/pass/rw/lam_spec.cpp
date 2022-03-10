#include "thorin/pass/rw/lam_spec.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"

namespace thorin {

const Def* LamSpec::rewrite(const Def* def) {
    if (auto new_def = old2new_.lookup(def)) return *new_def;

    if (auto [app, old_lam] = isa_apped_nom_lam(def); isa_workable(old_lam)) {
        app->dump(0);
        std::vector<const Def*> new_doms, new_vars, new_args;
        auto skip     = old_lam->ret_var() ? 1 : 0;
        auto old_doms = old_lam->doms();

        for (auto dom : old_doms.skip_back(skip)) {
            if (!dom->isa<Pi>()) new_doms.emplace_back(dom);
        }

        if (skip) new_doms.emplace_back(old_lam->doms().back());
        if (new_doms.size() == old_lam->num_doms()) return def;

        auto new_pi  = world().cn(world().sigma(new_doms));
        auto new_lam = old_lam->stub(world(), new_pi, old_lam->dbg());

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
            new_vars.emplace_back(old_lam->vars().back());
            new_args.emplace_back(app->args().back());
        }

        outln("old_args: {, }", app->args());
        outln("new_args: {, }", new_args);
        outln("old_vars: {, }", old_lam->vars());
        outln("new_vars: {, }", new_vars);

        new_lam->set(old_lam->reduce(world().tuple(new_vars)));
        world().DLOG("{} -> {}: {} -> {})", old_lam, new_lam, old_lam->dom(), new_lam->dom());

        return old2new_[def] = world().app(new_lam, new_args);
    }

    return def;
}

} // namespace thorin
