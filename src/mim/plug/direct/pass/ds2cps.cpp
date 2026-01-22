#include "mim/plug/direct/pass/ds2cps.h"

#include <iostream>

#include <mim/lam.h>

#include "mim/rewrite.h"

#include "mim/plug/direct/direct.h"

namespace mim::plug::direct {

const Def* DS2CPS::rewrite(const Def* def) {
    world().log().set(mim::Log::Level::Debug);
    static bool written = false;
    if (!written) {
        written = true;
        world().write("before_ds2cps.mim");
    }
    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>()) {
            DLOG("encountered lam app");
            auto new_lam = rewrite_lam(lam);
            if(new_lam == lam) {
                DLOG("lam unchanged");
                return def;
            }
            DLOG("new lam: {} : {}", new_lam, new_lam->type());
            DLOG("arg: {} : {}", app->arg(), app->arg()->type());
            auto new_app = world().app(new_lam, app->arg());
            DLOG("new app: {} : {}", new_app, new_app->type());
            world().write("after_ds2cps.mim");
            world().log().set(mim::Log::Level::Debug);
            return new_app;
        }
    }
    world().log().set(mim::Log::Level::Info);
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

    DLOG("rewrite DS function {} : {}", lam, lam->type());

    auto ty    = lam->type();
    auto var   = ty->has_var();
    auto dom   = ty->dom();
    auto codom = ty->codom();
    auto sigma = world().mut_sigma(2);
    // replace ds dom var with cps sigma var (cps dom)
    auto rw_codom = var ? VarRewriter(var, sigma->var(2, 0)).rewrite(codom) : codom;
    sigma->set(0, dom);
    sigma->set(1, world().cn(rw_codom));

    DLOG("original codom: {}", codom);
    DLOG("rewritten codom: {}", rw_codom);

    auto cps_lam            = world().mut_con(sigma)->set(lam->sym().str() + "_cps");
    auto [filter, cps_body] = lam->reduce<2>(cps_lam->var(0_n));

    // rewrite vars of new function
    // calls handled separately
    DLOG("body: {} : {}", lam->body(), lam->body()->type());
    DLOG("cps body: {} : {}", cps_body, cps_body->type());

    cps_lam->app(filter, cps_lam->vars().back(), cps_body);

    rewritten_[lam] = op_cps2ds_dep(cps_lam);
    DLOG("replace {} : {}", lam, lam->type());
    DLOG("with {} : {}", rewritten_[lam], rewritten_[lam]->type());

    return rewritten_[lam];
}

} // namespace mim::plug::direct
