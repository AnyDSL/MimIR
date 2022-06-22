#include "dialects/clos/normalize.h"

namespace thorin::clos {

template<Clos o>
const Def* normalize_Clos(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& w = type->world();
    return o == Clos::bot ? arg : w.raw_app(callee, arg, dbg);
}

} // namespace thorin::clos
