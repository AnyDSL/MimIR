#include "thorin/plug/clos/clos.h"

namespace thorin::plug::clos {

template<attr o> Ref normalize_clos(Ref type, Ref callee, Ref arg) {
    auto& w = type->world();
    return o == attr::bot ? arg : w.raw_app(type, callee, arg);
}

THORIN_clos_NORMALIZER_IMPL

} // namespace thorin::plug::clos
