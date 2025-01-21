#pragma once

#include <mim/world.h>

#include "mim/plug/tensor/autogen.h"

namespace mim::plug::tensor {

inline bool is_zero(Ref zero, Ref def) {
    if (zero == def) return true;
    if (auto pack = def->isa<Pack>()) return is_zero(zero, pack->body());
    return false;
}

inline Ref mk_zero(Ref zero, nat_t n, nat_t m) {
    auto& world = zero->world();
    return world.pack({n, m}, zero);
}

} // namespace mim::plug::tensor
