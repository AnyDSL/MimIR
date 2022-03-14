#include "thorin/pass/rw/lower_for.h"

#include "thorin/lam.h"
#include "thorin/tables.h"

namespace thorin {

const Def* LowerFor::rewrite(const Def* def) {
    if (auto app = def->isa<App>())
        if (auto for_ax = app->axiom(); for_ax && for_ax->tag() == Tag::For) {
            auto& w = def->world();
            w.DLOG("rewriting for axiom: {} within {}", app, curr_nom());
            
            auto for_pi  = app->callee_type();
            auto for_lam = w.nom_lam(for_pi, w.dbg("for"));

            auto body      = app->args()[app->num_args() - 2];
            auto body_type = body->type()->as<Pi>();
            auto cont_pi   = body_type->dom(body_type->num_doms() - 1)->as<Pi>();
            auto cont_lam  = w.nom_lam(cont_pi, w.dbg("continue"));

            { // construct cont
                auto [mem, iter, stop, step, acc, body, brk] =
                    for_lam->vars<7>({w.dbg("mem"), w.dbg("start"), w.dbg("stop"), w.dbg("step"), w.dbg("acc"),
                                      w.dbg("body"), w.dbg("break")});
                auto [cont_mem, cont_acc] = cont_lam->vars<2>();

                auto add = w.op(Wrap::add, w.lit_nat_0(), iter, step);
                cont_lam->app(false, for_lam, {cont_mem, add, stop, step, cont_acc, body, brk});
            }
            { // construct for
                auto [mem, iter, stop, step, acc, body, brk] = for_lam->vars<7>();

                // continue
                auto if_then_cn = w.cn(w.type_mem());
                auto if_then    = w.nom_lam(if_then_cn, nullptr);
                if_then->app(false, body, {if_then->mem_var(w.dbg("mem")), iter, acc, cont_lam});

                // break
                auto if_else_cn = w.cn(w.type_mem());
                auto if_else    = w.nom_lam(if_else_cn, nullptr);
                if_else->app(false, brk, {if_else->mem_var(w.dbg("mem")), acc});

                auto cmp    = w.op(ICmp::sl, iter, stop);
                auto select = w.select(if_then, if_else, cmp);
                for_lam->app(false, select, mem);
            }

            return app->rebuild(w, nullptr, {for_lam, app->op(1)}, app->dbg());
        }

    return def;
}

} // namespace thorin
