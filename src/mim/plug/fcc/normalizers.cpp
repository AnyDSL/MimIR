#include "mim/world.h"

#include "mim/plug/fcc/fcc.h"

namespace mim::plug::fcc {

Ref normalize_code(Ref type, Ref, Ref cl) {
    auto& world = type->world();
    if (auto clos = match<fcc::clos>(cl)) {
        // TODO
    }
    return {};
}

MIM_fcc_NORMALIZER_IMPL

} // namespace mim::plug::fcc
