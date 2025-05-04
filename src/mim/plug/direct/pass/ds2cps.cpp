#include "mim/plug/direct/pass/ds2cps.h"

#include <iostream>

#include <mim/lam.h>

#include "mim/rewrite.h"

#include "mim/plug/direct/direct.h"

namespace mim::plug::direct {

const Def* DS2CPS::rewrite(const Def* def) {
    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>()) {
            world().DLOG("encountered lam app");
            auto new_lam = rewrite_lam(lam);
            world().DLOG("new lam: {} : {}", new_lam, new_lam->type());
            world().DLOG("arg: {} : {}", app->arg(), app->arg()->type());
            auto new_app = world().app(new_lam, app->arg());
            world().DLOG("new app: {} : {}", new_app, new_app->type());
            return new_app;
        }
    }
    return def;
}

/// This function generates the cps function `f_cps : cn [a:A, cn B]` for a ds function `f: [a : A] -> B`.
/// The translation is associated in the `rewritten_` map.
const Def* DS2CPS::rewrite_lam(Lam* lam) {
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
    auto var   = ty->has_var();
    auto dom   = ty->dom();
    auto codom = ty->codom();
    auto sigma = world().mut_sigma(2);
    // replace ds dom var with cps sigma var (cps dom)
    auto rw_codom = var ? VarRewriter(var, sigma->var(2, 0)).rewrite(codom) : codom;
    sigma->set(0, dom);
    sigma->set(1, world().cn(rw_codom));

    world().DLOG("original codom: {}", codom);
    world().DLOG("rewritten codom: {}", rw_codom);

    auto cps_lam = world().mut_con(sigma)->set(lam->sym().str() + "_cps");

    // rewrite vars of new function
    // calls handled separately
    world().DLOG("body: {} : {}", lam->body(), lam->body()->type());

    auto new_ops  = lam->reduce(cps_lam->var(0_n));
    auto filter   = new_ops[0];
    auto cps_body = new_ops[1];

    world().DLOG("cps body: {} : {}", cps_body, cps_body->type());

    cps_lam->app(filter, cps_lam->vars().back(), cps_body);

    rewritten_[lam] = op_cps2ds_dep(cps_lam);
    world().DLOG("replace {} : {}", lam, lam->type());
    world().DLOG("with {} : {}", rewritten_[lam], rewritten_[lam]->type());

    return rewritten_[lam];
}

} // namespace mim::plug::direct
