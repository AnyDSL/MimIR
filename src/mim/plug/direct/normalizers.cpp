#include <mim/world.h>

#include "mim/plug/direct/direct.h"
#include "mim/plug/direct/pass/cps2ds.h"
#include "mim/plug/direct/pass/ds2cps.h"

namespace mim::plug::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
const Def* normalize_cps2ds(const Def*, const Def*, const Def* fun) { return op_cps2ds_dep(fun); }

template<pass id>
const Def* normalize_pass(const Def* t, const Def*, const Def*) {
    switch (id) {
        case pass::cps2ds: return create<CPS2DS>(id, t);
        case pass::ds2cps: return create<DS2CPS>(id, t);
    }
}

MIM_direct_NORMALIZER_IMPL

} // namespace mim::plug::direct
