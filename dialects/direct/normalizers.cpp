#include "thorin/world.h"

#include "dialects/direct/direct.h"

namespace thorin::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
const Def* normalize_cps2ds(const Def*, const Def*, const Def* fun, const Def* dbg) { return op_cps2ds_dep(fun, dbg); }

THORIN_direct_NORMALIZER_IMPL

} // namespace thorin::direct
