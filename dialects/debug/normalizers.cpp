#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/debug/debug.h"

namespace thorin::debug {

const Def* normalize_dbg(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    debug_print(arg);
    return arg;
}

const Def* normalize_dbg_perm(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    debug_print(arg);
    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_force_type(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // debug_print("force_type", arg);
    if (auto app = arg->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->arg();
        if (auto lam = callee->isa<Lam>()) {
            auto res = lam->reduce(args).back(); // ops => filter, result
            world.DLOG("force {} : {} with {} : {}", lam, lam->type(), args, args->type());
            world.DLOG("res {}", res);
            return res;
        }
    }
    return world.raw_app(callee, arg, dbg);
}

THORIN_debug_NORMALIZER_IMPL

} // namespace thorin::debug
