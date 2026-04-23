#include "mim/world.h"

#include "mim/plug/sflow/autogen.h"

namespace mim::plug::sflow {

const Def* normalize_const(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

const Def* normalize_branch(const Def* type, const Def* callee, const Def* args) {
    auto& world                            = type->world();
    auto [n, Ts, merge, tuple, index, arg] = App::uncurry_args<6>(callee);

    if (Lit::as(n) == 1) return world.app(world.extract(tuple, index), arg);

    return world.raw_app(type, callee, args);
}

/// Normalizer for %sflow.Targets.
/// When n is a known literal, unfolds to a flat sigma:
///   Targets 0 next = []
///   Targets n next = [Nat, Cn [T, Struct header next break],
///                     Nat, Cn [T, Struct header case₀ break], ...]
/// Each case's continue target is the previous case (fallthrough).
const Def* normalize_targets(const Def*, const Def* callee, const Def* next) {
    auto& world             = next->world();
    auto [implicits, n_def] = App::uncurry_args<2>(callee);
    auto n_lit              = Lit::isa(n_def);
    std::cerr << "normalize_targets called, n_lit = " << (n_lit ? std::to_string(*n_lit) : "none") << std::endl;
    if (!n_lit) return {};

    auto n = *n_lit;
    if (n == 0) return world.sigma();

    // Extract implicit args: {T B H: ★, header: Cn H, continue: Cn T, break: Cn B}
    auto T      = implicits->proj(6, 0);
    auto B      = implicits->proj(6, 1);
    auto H      = implicits->proj(6, 2);
    auto header = implicits->proj(6, 3);
    auto brk    = implicits->proj(6, 5);

    // Build Struct type: %sflow.Struct header continue break
    auto struct_axm = world.annex<sflow::Struct>();

    // Build sigma of tuples: [[Nat, Cn], [Nat, Cn], ...]
    DefVec ops;
    ops.reserve(n);

    auto continue_target = next;
    for (size_t i = 0; i < n; ++i) {
        auto struct_ty  = world.app(world.app(world.app(struct_axm, {H, T, B}), header), continue_target);
        auto struct_ty2 = world.app(struct_ty, brk);
        auto case_dom   = world.sigma({T, struct_ty2});
        auto case_ty    = world.cn(case_dom);

        ops.push_back(world.sigma({world.type_nat(), case_ty}));

        continue_target = case_ty;
    }

    return world.sigma(ops);
}

const Def* normalize_switch(const Def* type, const Def* callee, const Def* arg) { return {}; }
const Def* normalize_loop(const Def* type, const Def* callee, const Def* arg) { return {}; }

MIM_sflow_NORMALIZER_IMPL

} // namespace mim::plug::sflow
