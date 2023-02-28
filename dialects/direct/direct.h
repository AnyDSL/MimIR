#pragma once

#include "thorin/world.h"

#include "dialects/direct/autogen.h"

namespace thorin::direct {

inline const Def* op_cps2ds_dep(const Def* f) {
    auto& world = f->world();
    // TODO: assert continuation
    world.DLOG("f: {} : {}", f, f->type());
    auto f_ty = f->type()->as<Pi>();
    auto T    = f_ty->dom(0);
    auto U    = f_ty->dom(1)->as<Pi>()->dom();
    world.DLOG("T: {}", T);
    world.DLOG("U: {}", U);

    auto Uf = world.nom_lam(world.pi(T, world.type()))->set(world.sym("Uf"));
    world.DLOG("Uf: {} : {}", Uf, Uf->type());

    const Def* rewritten_codom;

    if (auto f_ty_sig = f_ty->dom()->isa_nom<Sigma>()) {
        auto dom_var = f_ty_sig->var((nat_t)0);
        world.DLOG("dom_var: {}", dom_var);
        Scope r_scope{f_ty_sig};
        auto closed_dom_var = Uf->var();
        rewritten_codom     = thorin::rewrite(U, dom_var, closed_dom_var, r_scope);
    } else {
        rewritten_codom = U;
    }
    Uf->set_filter(true);
    Uf->set_body(rewritten_codom);

    auto ax_app = world.app(world.ax<direct::cps2ds_dep>(), {T, Uf});

    world.DLOG("axiom app: {} : {}", ax_app, ax_app->type());

    return world.app(ax_app, f);
}

} // namespace thorin::direct
