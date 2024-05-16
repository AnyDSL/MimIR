#include "thorin/lattice.h"

#include "thorin/util/util.h"

namespace thorin {

size_t Bound::find(const Def* type) const {
    auto i = isa_mut() ? std::find(ops().begin(), ops().end(), type)
                       : binary_find(ops().begin(), ops().end(), type, GIDLt<const Def*>());
    return i == ops().end() ? size_t(-1) : i - ops().begin();
}

} // namespace thorin
