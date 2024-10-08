#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

template<attr o> Ref normalize_clos(Ref type, Ref callee, Ref arg) {
    auto& w = type->world();
    return o == attr::bot ? arg : w.raw_app(type, callee, arg);
}

MIM_clos_NORMALIZER_IMPL

} // namespace mim::plug::clos
