#include "mim/world.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

MIM_tensor_NORMALIZER_IMPL

} // namespace mim::plug::tensor
