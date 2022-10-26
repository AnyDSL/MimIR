#include <iostream>

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/core/core.h"
// #include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

/// Currently this normalizer does nothin.
/// TODO: Maybe we want to handle trivial lookup replacements here.
const Def* normalize_ad(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_AD(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto ad_ty  = autodiff_type_fun(arg);
    if (ad_ty) return ad_ty;
    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_Tangent(const Def*, const Def*, const Def* arg, const Def*) { return tangent_type_fun(arg); }

const Def* normalize_zero(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_add(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    // TODO: add tuple -> tuple of adds
    // TODO: add zero -> other
    // TODO: unify mapping over structure with other aspects like zero

    auto T      = callee->as<App>()->arg();
    auto [a, b] = arg->projs<2>();

    world.DLOG("add {} {} {}", T, a, b);

    if (match<zero>(a)) {
        world.DLOG("0+b");
        return b;
    }
    if (match<zero>(b)) {
        world.DLOG("0+a");
        return a;
    }
    if (auto sig = T->isa<Sigma>()) {
        world.DLOG("add tuple");
        auto p = sig->num_ops(); // TODO: or num_projs
        DefArray ops(p, [&](size_t i) {
            return world.app(world.app(world.ax<add>(), sig->op(i)), {a->proj(i), b->proj(i)});
        });
        return world.tuple(ops);
    } else if (auto arr = T->isa<Arr>()) {
        world.DLOG("add arrays {} {} {}", T, a, b);
        auto pack      = world.nom_pack(T);
        auto body_type = arr->body();
        world.DLOG("body type {}", body_type);
        pack->set(world.app(world.app(world.ax<add>(), body_type),
                            {world.extract(a, pack->var()), world.extract(b, pack->var())}));
        world.DLOG("pack {}", pack);
        return pack;
    } else if (auto ptr = match<mem::Ptr>(T)) {
        // TODO: see review
        return a;
    } else if (auto mem = match<mem::M>(T)) {
        // TODO: see review
        return world.top(mem::type_mem(world));
    } else if (Idx::size(type)) {
        world.DLOG("add int");
        auto width = as_lit(world.iinfer(a));
        world.DLOG("width {}", width);
        auto int_add = core::op(core::wrap::add, core::WMode::none, a, b);
        world.DLOG("int add {} : {}", int_add, world.iinfer(int_add));
        return int_add;
    } else if (auto real = match<core::Real>(T)) {
        auto width = as_lit<nat_t>(real->arg());
        world.DLOG("width {}", width);
        auto real_add =
            world.app(world.app(world.ax(core::rop::add), {world.lit_nat_0(), world.lit_nat(width)}), {a, b});
        world.DLOG("real add {} : {}", real_add, real_add->type());
        return real_add;

    } else if (auto app = T->isa<App>()) {
        auto callee = app->callee();
        assert(0 && "not handled");
    }
    // TODO: mem stays here (only resolved after direct simplification)

    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_sum(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // sum (n,T) arr

    auto [count, T] = callee->as<App>()->args<2>();

    if (auto lit = count->isa<Lit>()) {
        auto val = lit->get<nat_t>();
        world.DLOG("val: {}", val);
        DefArray args = arg->projs(val);
        auto sum      = op_zero(T);
        // This special case would also be handled by add zero.
        if (val >= 1) { sum = args[0]; }
        for (size_t i = 1; i < val; ++i) sum = world.app(world.app(world.ax<add>(), T), {sum, args[i]});
        return sum;
    }
    assert(0);

    return world.raw_app(callee, arg, dbg);
}

THORIN_autodiff_NORMALIZER_IMPL

} // namespace thorin::autodiff
