#include "mim/plug/direct/direct.h"
#include "mim/world.h"

namespace mim::plug::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
Ref normalize_cps2ds(Ref, Ref, Ref fun) { return op_cps2ds_dep(fun); }

THORIN_direct_NORMALIZER_IMPL

} // namespace mim::plug::direct
