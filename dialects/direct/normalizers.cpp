#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/direct/direct.h"

namespace thorin::direct {

const Def* normalize_cps2ds(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // TODO: call (op_cps2ds_dep f)
    assert(false && "not implemented");
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
