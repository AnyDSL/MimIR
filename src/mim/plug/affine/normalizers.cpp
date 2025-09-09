#include "mim/plug/affine/affine.h"
#include "mim/plug/affine/pass/lower_for.h"

namespace mim::plug::affine {

template<pass>
const Def* normalize_pass(const Def* t, const Def*, const Def*) {
    outln("XXX");
    return create<LowerFor>(pass::lower_for, t);
}

MIM_affine_NORMALIZER_IMPL

} // namespace mim::plug::affine
