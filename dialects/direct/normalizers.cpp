#include "thorin/world.h"

#include "dialects/direct/direct.h"

namespace thorin::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
const Def* normalize_cps2ds(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    // Collect all arguments of the application.
    // For example in
    // `cps2ds (A,B) f x`
    // `arg` is `x`
    // Example2:
    // `cps2ds (A,B->C) f x y`
    // `arg` is `y`
    // `callee` is `cps2ds (A,B->C) f x`
    // We are mainly interested in `f` here.
    auto [ax, curry_args] = collect_args(callee);
    curry_args.push_back(arg);
    // The function is the second argument (after the type tuple).
    auto fun = curry_args[1];
    auto r   = op_cps2ds_dep(fun);
    for (size_t i = 2; i < curry_args.size(); ++i) r = world.app(r, curry_args[i]);
    return r;

    return world.raw_app(callee, arg, dbg);
}

THORIN_direct_NORMALIZER_IMPL

} // namespace thorin::direct
