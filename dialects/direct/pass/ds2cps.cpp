#include "dialects/direct/pass/ds2cps.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

Ref DS2CPS::rewrite(Ref def) {
    auto& world = def->world();
    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>()) {
            world.DLOG("encountered lam app");
            auto new_lam = rewrite_lam(lam);
            world.DLOG("new lam: {} : {}", new_lam, new_lam->type());
            world.DLOG("arg: {} : {}", app->arg(), app->arg()->type());
            auto new_app = world.app(new_lam, app->arg());
            world.DLOG("new app: {} : {}", new_app, new_app->type());
            return new_app;
        }
    }
    return def;
}

/// This function generates the cps function `f_cps : cn [a:A, cn B]` for a ds function `f: Π a : A -> B`.
/// The translation is associated in the `rewritten_` map.
Ref DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return i->second;

    // only look at lambdas (ds not cps)
    if (Lam::isa_cn(lam)) return lam;
    // ignore ds on type level
    if (lam->type()->codom()->isa<Type>()) return lam;
    // ignore higher order function
    if (lam->type()->codom()->isa<Pi>()) {
        // We can not set the filter here as this causes segfaults.
        return lam;
    }

    world().DLOG("rewrite DS function {} : {}", lam, lam->type());

    auto ty    = lam->type();
    auto dom   = ty->dom();
    auto codom = ty->codom();
    auto sigma = world().mut_sigma(2);
    sigma->set(0, dom);

    Ref rewritten_codom;
    if (auto mut = ty->isa_mut()) {
        // replace ds dom var with cps sigma var (cps dom)
        Scope r_scope{mut};
        auto dom_var     = mut->var();
        auto cps_dom_var = sigma->var(2, 0);
        rewritten_codom  = thorin::rewrite(codom, dom_var, cps_dom_var, r_scope);
    } else {
        rewritten_codom = codom;
    }
    sigma->set(1, world().cn(rewritten_codom));

    world().DLOG("original codom: {}", codom);
    world().DLOG("rewritten codom: {}", rewritten_codom);

    auto cps_ty = world().cn(sigma);
    world().DLOG("cps type: {}", cps_ty);

    auto cps_lam = world().mut_lam(cps_ty)->set(*lam->sym() + "_cps");

    // rewrite vars of new function
    // calls handled separately
    Scope l_scope{lam};
    world().DLOG("body: {} : {}", lam->body(), lam->body()->type());

    auto cps_body = thorin::rewrite(lam->body(), lam->var(), cps_lam->var(0_n), l_scope);

    world().DLOG("cps body: {} : {}", cps_body, cps_body->type());

    cps_lam->app(lam->filter(), cps_lam->vars().back(), cps_body);

    rewritten_[lam] = op_cps2ds_dep(cps_lam);
    world().DLOG("replace {} : {}", lam, lam->type());
    world().DLOG("with {} : {}", rewritten_[lam], rewritten_[lam]->type());

    return rewritten_[lam];
}

} // namespace thorin::direct
