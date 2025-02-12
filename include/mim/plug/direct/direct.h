#pragma once

#include <mim/rewrite.h>

#include "mim/plug/direct/autogen.h"

namespace mim::plug::direct {

// TODO remove
inline Ref rewrite(Ref def, Def* mut, Ref arg) { return VarRewriter(mut->var(), arg).rewrite(def); }

/// @name %%direct.cps2ds_dep
///@{
inline const Def* op_cps2ds_dep(const Def* f) {
    auto& world = f->world();
    // TODO: assert continuation
    world.DLOG("f: {} : {}", f, f->type());
    auto f_ty = f->type()->as<Pi>();
    auto T    = f_ty->dom(0);
    auto U    = f_ty->dom(1)->as<Pi>()->dom();
    world.DLOG("T: {}", T);
    world.DLOG("U: {}", U);

    auto Uf = world.mut_lam(T, world.type())->set("Uf");
    world.DLOG("Uf: {} : {}", Uf, Uf->type());

    const Def* rewritten_codom;

    if (auto f_ty_sig = f_ty->dom()->isa_mut<Sigma>()) {
        auto dom_var = f_ty_sig->var((nat_t)0);
        world.DLOG("dom_var: {}", dom_var);
        auto closed_dom_var = Uf->var();
        rewritten_codom     = rewrite(U, f_ty_sig, closed_dom_var);
    } else {
        rewritten_codom = U;
    }
    Uf->set(true, rewritten_codom);

    auto ax_app = world.app(world.annex<direct::cps2ds_dep>(), {T, Uf});

    world.DLOG("axiom app: {} : {}", ax_app, ax_app->type());

    return world.app(ax_app, f);
}
///@}

} // namespace mim::plug::direct
