#include "thorin/world.h"

#include "plug/direct/direct.h"

namespace thorin::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
Ref normalize_cps2ds(Ref, Ref, Ref fun) { return op_cps2ds_dep(fun); }

THORIN_direct_NORMALIZER_IMPL

} // namespace thorin::direct
