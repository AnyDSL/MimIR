#include "dialects/direct/passes/ds2cps.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

const Def* DS2CPS::rewrite(const Def* def) {
    auto& world = def->world();
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        if (auto lam = callee->isa_nom<Lam>()) {
            world.DLOG("encountered lam app");
            auto new_lam = rewrite_lam(lam);
            world.DLOG("new lam: {} : {}", new_lam, new_lam->type());
            auto arg = app->arg();
            world.DLOG("arg: {} : {}", arg, arg->type());
            auto new_app = world.app(new_lam, app->arg());
            world.DLOG("new app: {} : {}", new_app, new_app->type());
            return new_app;
        }
    }
    return def;
}

/// This function generates the cps function `f_cps : cn [a:A, cn B]` for a ds function `f: Π a : A -> B`.
/// The translation is associated in the `rewritten_` map.
const Def* DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return i->second;

    // only look at lambdas (ds not cps)
    if (lam->type()->is_cn()) { return lam; }
    // ignore ds on type level
    if (lam->type()->codom()->isa<Type>()) { return lam; }
    // ignore higher order function
    if (lam->type()->codom()->isa<Pi>()) {
        // We can not set the filter here as this causes segfaults.
        return lam;
    }

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

    auto cps_lam = world().nom_lam(cps_ty)->set(lam->name());
    cps_lam->debug_suffix("_cps");

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

} // namespace thorin::direct
