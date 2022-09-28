#include "thorin/world.h"

#include "dialects/direct/direct.h"

namespace thorin::direct {

std::pair<const Def*, std::vector<const Def*>> collect_args(const Def* def) {
    std::vector<const Def*> args;
    if (auto app = def->isa<App>()) {
        auto callee               = app->callee();
        auto arg                  = app->arg();
        auto [inner_callee, args] = collect_args(callee);
        // args.insert(args.begin(), arg);
        args.push_back(arg);
        return {inner_callee, args};
    } else {
        return {def, args};
    }
}

/// call (op_cps2ds_dep f)
const Def* normalize_cps2ds(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    // world.DLOG("arg: {} : {}", arg, arg->type());
    auto [ax, curry_args] = collect_args(callee);
    curry_args.push_back(arg);
    // world.DLOG("curry_args: { , }", curry_args);
    // world.DLOG("curry body: {} : {}", ax, ax->type());
    auto fun = curry_args[1];
    auto r   = op_cps2ds_dep(fun);
    for (int i = 2; i < curry_args.size(); i++) { r = world.app(r, curry_args[i]); }
    return r;

    // cps2ds types fun
    // but returns function =>
    // cps2ds types fun arg
    // ^ callee         ^ arg

    // world.DLOG("arg {} : {}", arg, arg->type());
    // auto [T,U] = callee->as<App>()->arg()->projs<2>();
    // world.DLOG("T = {}", T);
    // world.DLOG("U = {}", U);
    // U->as_nom<Lam>()->set_filter(true);
    return world.raw_app(callee, arg, dbg);
}

THORIN_direct_NORMALIZER_IMPL

} // namespace thorin::direct
