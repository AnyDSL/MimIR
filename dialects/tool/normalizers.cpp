#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/tool/tool.h"

namespace thorin::tool {

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

const Def* normalize_set_filter(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // auto [filter, lam] = arg->projs<2>();

    return world.raw_app(callee, arg, dbg);
}

THORIN_tool_NORMALIZER_IMPL

} // namespace thorin::tool
