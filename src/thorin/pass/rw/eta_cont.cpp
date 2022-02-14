
#include "thorin/pass/rw/eta_cont.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

const Def* EtaCont::rewrite(const Def* def) {
    auto& w = world();
    if (auto app = def->isa<App>(); app && app->callee_type()->is_returning()) {
        auto ret_arg = app->arg(app->num_args() - 1);
        if (isa<Tag::CA>(CA::ret, ret_arg)) return def;
        if (auto [_, lam] = ca_isa_var<Lam>(ret_arg); lam && ret_arg == lam->ret_var()) return def;
        Lam* cont;
        if (auto lam = ret_arg->isa_nom<Lam>()) {
            cont = lam;
        } else {
            auto [entry, inserted] = old2wrapper_.emplace(def, nullptr);
            auto& wrapper = entry->second;
            if (inserted) {
                wrapper = w.nom_lam(ret_arg->type()->as<Pi>(), w.dbg("eta_cont"));
                wrapper->app(ret_arg, wrapper->var());
            }
            cont = wrapper;
            w.DLOG("eta expanded return cont: {} -> {} in {}", ret_arg, cont, def);
        }
        auto new_arg = app->arg()->refine(app->num_args() - 1, w.ca_mark(cont, CA::ret));
        return w.app(app->callee(), new_arg, app->dbg());
    }
    return def;
}

}
