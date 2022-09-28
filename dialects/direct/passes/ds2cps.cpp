#include "dialects/direct/passes/ds2cps.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

// void DS2CPS::enter() {
//     Lam* lam = curr_nom();
//     if (!lam->isa_nom()) {
//         lam->world().DLOG("skipped non-nom {}", lam);
//         return;
//     }
//     rewrite_lam(lam);

//     // Scope scope{lam};
//     // ScopeRewriter rewriter(world(), scope);
//     // rewriter.old2new.insert(rewritten_.begin(), rewritten_.end());
//     // auto body = lam->body();
//     // auto b    = rewriter.rewrite(body);
//     // lam->set_body(b);
//     // world().ILOG("rewrote {}", lam);
//     // if (body != b) world().ILOG("changed body of {}", lam);
// }

const Def* DS2CPS::rewrite(const Def* def) {
    auto& world = def->world();
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->args();

        // pre-order!
        auto new_arg = rewrite_(app->arg());

        // manual unfolding instead of match<cps2ds>(callee) due to currying
        // TODO can we somehow enhance match to do this?
        Lam* conv_cps       = nullptr;
        auto [axiom, curry] = Axiom::get(callee);
        if (axiom && axiom->base() == Axiom::Base<cps2ds>) conv_cps = callee->as<App>()->arg()->as_nom<Lam>();

        if ((!axiom && !callee->type()->as<Pi>()->is_cn()) || conv_cps) {
            /*
            h:
              b = f a
              C[b]

            =>

            h:
                f'(a,h_cont)

            h_cont(b):
                C[b]

            f : A -> B
            f': .Cn [A, ret: .Cn[B]]
            */

            auto ty     = callee->type();
            auto ret_ty = ty->as<Pi>()->codom();

            const Def* lam_cps;
            if (conv_cps) {
                lam_cps = conv_cps;
            } else {
                // "real" ds function
                lam_cps = rewrite_(callee);
            }

            // continuation of call site to receive result
            auto fun_cont = world.nom_lam(world.cn(ret_ty), world.dbg(curr_lam_->name() + "_cont"));
            fun_cont->set_filter(curr_lam_->filter());

            // f a -> f_cps(a,cont)
            auto cps_call = world.app(lam_cps, {new_arg, fun_cont}, world.dbg("cps_call"));
            if (curr_lam_->body()) rewritten_bodies_[curr_lam_->body()] = cps_call;
            world.DLOG("  overwrite body {} of {} : {} with {} : {}", curr_lam_->body(), curr_lam_, curr_lam_->type(),
                       cps_call->unique_name(), cps_call->type());

            curr_lam_->set_body(cps_call);
            // Fixme: would be great to PE the newly added overhead away..
            // The current PE just does not terminate on loops.. :/
            // curr_lam_->set_filter(true);

            // write the body context in the newly created continuation
            // that has access to the result (as its argument)
            curr_lam_ = fun_cont;
            // result of ds function
            auto res = fun_cont->var();

            world.DLOG("  result {} : {} instead of {} : {}\n", res, res->type(), def, def->type());
            // replace call with the result in the context that will be placed in the continuation
            return res;
        }
    }
    return def;
}

/// generates the cps function `f_cps : cn [a:A, cn B]`
/// for a ds function `f: Π a : A -> B`
/// associates the functions in `rewritten_`.
const Def* DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return i->second;

    // only look at lambdas (ds not cps)
    if (lam->type()->codom()->isa<Bot>()) { return lam; }
    // ignore ds on type level
    if (lam->type()->codom()->isa<Type>()) { return lam; }
    // ignore higher order function
    if (lam->type()->codom()->isa<Pi>()) { return lam; }

    world().DLOG("rewrite DS function {} : {}", lam, lam->type());

    auto ty    = lam->type();
    auto dom   = ty->dom();
    auto codom = ty->codom();

    Sigma* sig = world().nom_sigma(2);
    sig->set(0, dom);

    // replace ds dom var with cps sigma var (cps dom)
    Scope r_scope{ty->as_nom()};
    auto dom_var         = ty->as_nom<Pi>()->var();
    auto cps_dom_var     = sig->var((nat_t)0);
    auto rewritten_codom = thorin::rewrite(codom, dom_var, cps_dom_var, r_scope);
    sig->set(1, world().cn(rewritten_codom));

    world().DLOG("original codom: {}", codom);
    world().DLOG("rewritten codom: {}", rewritten_codom);

    auto cps_ty = world().cn(sig);
    world().DLOG("cps type: {}", cps_ty);

    auto cps_lam = world().nom_lam(cps_ty, world().dbg(lam->name() + "_cps"));

    // rewrite vars of new function
    // calls handled separately
    Scope l_scope{lam};
    world().DLOG("body: {} : {}", lam->body(), lam->body()->type());

    auto cps_body = thorin::rewrite(lam->body(), lam->var(), cps_lam->var((nat_t)0), l_scope);

    world().DLOG("cps body: {} : {}", cps_body, cps_body->type());

    cps_lam->app(lam->filter(), cps_lam->vars().back(), cps_body);

    rewritten_[lam] = op_cps2ds_dep(cps_lam);
    world().DLOG("replace {} : {}", lam, lam->type());
    world().DLOG("with {} : {}", rewritten_[lam], rewritten_[lam]->type());

    return rewritten_[lam];
}

// const Def* DS2CPS::rewrite(const Def* def) {
//     world().DLOG("rewrite in ds2cps pass {} : {}", def, def->type());
//     if (rewritten_.find(def) != rewritten_.end()) {
//         world().DLOG("Rewrite: {} : {}", def, def->type());
//         world().DLOG("to {} : {}", rewritten_[def], rewritten_[def]->type());
//         assert(0);
//         return rewritten_[def];
//     }

//     return def;
// }

} // namespace thorin::direct
