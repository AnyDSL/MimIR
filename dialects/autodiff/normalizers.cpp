#include <iostream>

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/core/core.h"

namespace thorin::autodiff {

/// Currently this normalizer does nothin.
/// TODO: Maybe we want to handle trivial lookup replacements here.
Ref normalize_ad(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

Ref normalize_AD(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    auto ad_ty  = autodiff_type_fun(arg);
    if (ad_ty) return ad_ty;
    return world.raw_app(type, callee, arg);
}

Ref normalize_Tangent(Ref, Ref, Ref arg) { return tangent_type_fun(arg); }

/// Currently this normalizer does nothing.
/// We usually want to keep zeros as long as possible to avoid unnecessary allocations.
/// A high-level addition with zero can be shortened directly.
Ref normalize_zero(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

/// Currently resolved the full addition.
/// There is no benefit in keeping additions around longer than necessary.
Ref normalize_add(Ref type, Ref callee, Ref arg) {
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
    // A value level match would be harder as a tuple might in reality be a var or extract
    if (auto sig = T->isa<Sigma>()) {
        world.DLOG("add tuple");
        auto p = sig->num_ops(); // TODO: or num_projs
        DefArray ops(p, [&](size_t i) {
            return world.app(world.app(world.ax<add>(), sig->op(i)), {a->proj(i), b->proj(i)});
        });
        return world.tuple(ops);
    } else if (auto arr = T->isa<Arr>()) {
        // TODO: is this working for non-lit (non-tuple) or do we need a loop?
        world.DLOG("add arrays {} {} {}", T, a, b);
        auto pack      = world.mut_pack(T);
        auto body_type = arr->body();
        world.DLOG("body type {}", body_type);
        pack->set(world.app(world.app(world.ax<add>(), body_type),
                            {world.extract(a, pack->var()), world.extract(b, pack->var())}));
        world.DLOG("pack {}", pack);
        return pack;
    } else if (Idx::size(type)) {
        world.DLOG("add int");
        auto width = as_lit(world.iinfer(a));
        world.DLOG("width {}", width);
        auto int_add = world.call(core::wrap::add, 0_n, Defs{a, b});
        world.DLOG("int add {} : {}", int_add, world.iinfer(int_add));
        return int_add;
    } else if (T->isa<App>()) {
        assert(0 && "not handled");
    }
    // TODO: mem stays here (only resolved after direct simplification)

    return world.raw_app(type, callee, arg);
}

Ref normalize_sum(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();

    auto [count, T] = callee->as<App>()->args<2>();

    if (auto lit = count->isa<Lit>()) {
        auto val = lit->get<nat_t>();
        world.DLOG("val: {}", val);
        DefArray args = arg->projs(val);
        auto sum      = world.app(world.ax<zero>(), T);
        // This special case would also be handled by add zero
        if (val >= 1) sum = args[0];
        for (size_t i = 1; i < val; ++i) sum = world.app(world.app(world.ax<add>(), T), {sum, args[i]});
        return sum;
    }
    assert(0);

    return world.raw_app(type, callee, arg);
}

THORIN_autodiff_NORMALIZER_IMPL

} // namespace thorin::autodiff
