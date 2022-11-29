#include "thorin/world.h"

#include "dialects/direct/direct.h"

namespace thorin::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
const Def* normalize_cps2ds(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto fun    = arg;
    auto r      = op_cps2ds_dep(fun);
    return r;
}

THORIN_direct_NORMALIZER_IMPL

} // namespace thorin::direct
