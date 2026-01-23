#include "mim/plug/direct/phase/ds2cps.h"

#include <mim/lam.h>
#include <mim/world.h>

#include "mim/plug/direct/direct.h"

namespace mim::plug::direct {

const Def* DS2CPS::rewrite_imm_App(const App* app) {
    if (auto lam = app->callee()->isa_mut<Lam>()) {
        DLOG("encountered lam app");
        auto new_lam = rewrite_lam(lam);
        if (new_lam == lam) {
            DLOG("lam unchanged");
            return Rewriter::rewrite_imm_App(app);
        }
        DLOG("new lam: {} : {}", new_lam, new_lam->type());
        DLOG("arg: {} : {}", app->arg(), app->arg()->type());
        auto new_app = new_world().app(new_lam, rewrite(app->arg()));
        DLOG("new app: {} : {}", new_app, new_app->type());
        new_world().write("after_ds2cps.mim");
        return new_app;
    }

    return Rewriter::rewrite_imm_App(app);
}

/// This function generates the cps function `f_cps : Cn [a: A, Cn B]` for a ds function `f: [a : A] -> B`.
/// The translation is associated in the `rewritten_` map.
const Def* DS2CPS::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return i->second;

    // only look at lambdas (ds not cps)
    if (Lam::isa_cn(lam)) return rewrite(lam);
    // ignore ds on type level
    if (lam->type()->codom()->isa<Type>()) return rewrite(lam);
    // ignore higher order function
    if (lam->type()->codom()->isa<Pi>()) {
        // We can not set the filter here as this causes segfaults.
        return rewrite(lam);
    }

    DLOG("rewrite DS function {} : {}", lam, lam->type());

    auto pi    = lam->type();
    auto dom   = pi->dom();
    auto codom = pi->codom();
    auto sigma = new_world().mut_sigma(2);

    auto rw_codom = codom;
    if (auto var = pi->has_var()) {
        // replace ds dom var with cps sigma var (cps dom)
        push();
        map(var, sigma->var(2, 0));
        rw_codom = rewrite(codom);
        pop();
    }

    sigma->set(0, dom);
    sigma->set(1, new_world().cn(rw_codom));

    DLOG("original codom: {}", codom);
    DLOG("rewritten codom: {}", rw_codom);

    auto cps_lam = new_world().mut_con(sigma)->set(lam->dbg());
    cps_lam->debug_suffix("_cps");
    auto [filter, cps_body] = lam->reduce<2>(cps_lam->var(2, 0));

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
