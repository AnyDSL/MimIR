#include "mim/world.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

const Def* normalize_dot(const Def*, const Def*, const Def* ) {
    return nullptr;
}

MIM_tensor_NORMALIZER_IMPL

} // namespace mim::plug::tensor
