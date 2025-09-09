#include "mim/plug/clos/clos.h"
#include "mim/plug/clos/phase/clos_conv.h"
#include "mim/plug/clos/phase/lower_typed_clos.h"
#include "mim/plug/clos/pass/clos_conv_prep.h"
#include "mim/plug/clos/pass/branch_clos_elim.h"
#include "mim/plug/clos/pass/lower_typed_clos_prep.h"
#include "mim/plug/clos/pass/clos2sjlj.h"

namespace mim::plug::clos {

template<attr o>
const Def* normalize_clos(const Def*, const Def*, const Def* arg) {
    return o == attr::bottom ? arg : nullptr;
}

template<phase id>
const Def* normalize_phase(const Def* t, const Def*, const Def*) {
    switch (id) {
            // clang-format off
        case phase::clos_conv:        return create<ClosConv      >(id, t);
        case phase::lower_typed_clos: return create<LowerTypedClos>(id, t);
            // clang-format on
    }
}

template<pass id>
const Def* normalize_pass(const Def* t, const Def*, const Def*) {
    switch (id) {
            // clang-format off
        case pass::clos_conv_prep:        return create<ClosConvPrep      >(id, t);
        case pass::branch_clos:           return create<BranchClosElim    >(id, t);
        case pass::lower_typed_clos_prep: return create<LowerTypedClosPrep>(id, t);
        case pass::clos2sjlj:             return create<Clos2SJLJ         >(id, t);
            // clang-format on
    }
}

MIM_clos_NORMALIZER_IMPL

} // namespace mim::plug::clos
