#include "thorin/pass/fp/tail_rec_elim.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_red.h"

namespace thorin {

const Def* TailRecElim::rewrite(const Def* def) {
    if (auto [app, old] = isa_apped_nom_lam(def); old) {
        if (auto i = old2rec_loop_.find(old); i != old2rec_loop_.end()) {
            auto [rec, loop] = i->second;
            if (auto ret_var = rec->ret_var(); app->args().back() == ret_var)
                return world().app(loop, app->args().skip_back());
            else
                return world().app(rec, app->args());
        }
    }

    return def;
}

undo_t TailRecElim::analyze(const Def* def) {
    if (auto [app, old] = isa_apped_nom_lam(def); old) {
        if (auto ret_var = old->ret_var(); ret_var && app->args().back() == ret_var) {
            if (auto [i, ins] = old2rec_loop_.emplace(old, std::pair<Lam*, Lam*>(nullptr, nullptr)); ins) {
                auto& [rec, loop] = i->second;
                rec               = old->stub(world(), old->type(), old->dbg());
                auto doms         = rec->doms();
                auto loop_dom     = doms.skip_back();
                loop              = rec->stub(world(), world().cn(loop_dom), rec->dbg());
                world().DLOG("old {} -> (rec: {}, loop: {})", old, rec, loop);

                auto n = rec->num_doms();
                std::vector<const Def*> loop_args(n - 1);
                std::vector<const Def*> loop_vars(n);
                for (size_t i = 0; i != n - 1; ++i) {
                    loop_args[i] = rec->var(i);
                    loop_vars[i] = loop->var(i);
                }
                loop_vars.back() = rec->vars().back();

                loop->set(old->reduce(world().tuple(loop_vars)));
                rec->app(false, loop, loop_args);
                if (eta_red_) eta_red_->mark_irreducible(loop);

                return undo_visit(old);
            }
        }
    }

    return No_Undo;
}

} // namespace thorin
