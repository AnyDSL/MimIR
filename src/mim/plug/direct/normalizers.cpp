#include "mim/world.h"

#include "mim/plug/direct/direct.h"

namespace mim::plug::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
const Def* normalize_cps2ds(const Def*, const Def*, const Def* fun) { return op_cps2ds_dep(fun); }

MIM_direct_NORMALIZER_IMPL

} // namespace mim::plug::direct
