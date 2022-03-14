#include "thorin/pass/rw/lower_for.h"

#include "thorin/lam.h"

namespace thorin {

const Def* LowerFor::rewrite(const Def* def) {
    if (auto for_ax = isa<Tag::For>(def)) {
        auto& w = world();
        w.DLOG("rewriting for axiom: {} within {}", for_ax, curr_nom());

        auto for_pi  = for_ax->callee_type();
        auto for_lam = w.nom_lam(for_pi, w.dbg("for"));

        auto org_body  = for_ax->arg(for_ax->num_args() - 2);
        auto body_type = org_body->type()->as<Pi>();
        auto cont_pi   = body_type->doms().back()->as<Pi>();
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

            auto cmp = w.op(ICmp::sl, iter, stop);
            for_lam->branch(false, cmp, if_then, if_else, mem);
        }

        return w.app(for_lam, for_ax->arg(), for_ax->dbg());
    }

    return def;
}

} // namespace thorin
