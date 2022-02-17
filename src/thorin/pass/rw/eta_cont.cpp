
#include "thorin/pass/rw/eta_cont.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

const Def* EtaCont::eta_wrap_cont(const Def* cont_arg) {
    auto& w = world();
    if (isa<Tag::CA>(CA::ret, cont_arg)) return cont_arg;
    if (auto [_, lam] = ca_isa_var<Lam>(cont_arg); lam && cont_arg == lam->ret_var()) return cont_arg;
    if (auto lam = cont_arg->isa_nom<Lam>()) {
        return w.ca_mark(lam, CA::ret);
    } else {
        auto [entry, inserted] = old2wrapper_.emplace(cont_arg, nullptr);
        auto& wrapper = entry->second;
        if (inserted) {
            wrapper = w.nom_lam(cont_arg->type()->as<Pi>(), w.dbg("eta_cont"));
            wrapper->app(cont_arg, wrapper->var());
        }
        w.DLOG("eta expanded return cont: {} -> {}", cont_arg, wrapper);
        return w.ca_mark(wrapper, CA::ret);
    }
}

const Def* EtaCont::rewrite(const Def* def) {
    auto& w = world();
    if (auto app = curr_nom()->body()->as<App>(); app && def == app->callee())
        return def; // don't rewrite branches
    for (auto i = 0u; i < def->num_ops(); i++) {
        auto inner = def->op(i);
        if (isa_callee(def, i) || isa<Tag::CA>(inner)) continue;
        for (auto j = 0u; j < inner->num_ops(); j++) {
            if (inner->isa<Var>()) continue;
            if (auto app = def->isa<App>(); app && app->callee_type()->is_returning() && i == 1 && j == app->num_args() - 1) {
                inner = inner->refine(j, eta_wrap_cont(inner->op(j)));
            } else if (auto lam = inner->op(j)->isa_nom<Lam>(); lam && lam->is_basicblock()) {
                inner = inner->refine(j, w.ca_mark(lam, CA::unknown));
            }
        }
        def = def->refine(i, inner);
    }
    return def;
}

}
