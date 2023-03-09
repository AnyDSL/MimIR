#include "dialects/affine/passes/lower_for.h"

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/mem/mem.h"

namespace thorin::affine {

const Def* LowerFor::rewrite(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto for_ax = match<affine::For>(def)) {
        auto& w = world();
        w.DLOG("rewriting for axiom: {} within {}", for_ax, curr_nom());

        auto for_pi = for_ax->callee_type();
        DefArray for_dom{for_pi->num_doms() - 2, [&](size_t i) { return for_pi->dom(i); }};
        auto for_lam = w.nom_lam(w.cn(for_dom))->set("for");

        auto body = for_ax->arg(for_ax->num_args() - 2)->set("body");
        auto brk  = for_ax->arg(for_ax->num_args() - 1)->set("break");

        auto body_type = body->type()->as<Pi>();
        auto yield_pi  = body_type->doms().back()->as<Pi>();
        auto yield_lam = w.nom_lam(yield_pi)->set("yield");

        { // construct yield
            auto [iter, end, step, acc] = for_lam->vars<4>();
            iter->set("iter");
            end->set("end");
            step->set("step");
            acc->set("acc");
            auto yield_acc = yield_lam->var();

            auto add = w.call(core::wrap::add, 0_n, Defs{iter, step});
            yield_lam->app(false, for_lam, {add, end, step, yield_acc});
        }
        { // construct for
            auto [iter, end, step, acc] = for_lam->vars<4>();

            // reduce the body to remove the cn parameter
            auto nom_body = body->as_nom<Lam>();
            auto new_body = nom_body->stub(w, w.cn(w.sigma()))->set(body->dbg());
            new_body->set(nom_body->reduce(w.tuple({iter, acc, yield_lam})));

            // break
            auto if_else_cn = w.cn(w.sigma());
            auto if_else    = w.nom_lam(if_else_cn);
            if_else->app(false, brk, acc);

            auto cmp = w.call(core::icmp::ul, Defs{iter, end});
            for_lam->branch(false, cmp, new_body, if_else, w.tuple());
        }

        DefArray for_args{for_ax->num_args() - 2, [&](size_t i) { return for_ax->arg(i); }};
        return rewritten_[def] = w.app(for_lam, for_args);
    }

    return def;
}

} // namespace thorin::affine
