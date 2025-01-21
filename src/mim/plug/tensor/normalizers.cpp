#include "mim/world.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

Ref normalize_matmul(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();

    if (auto app = callee->isa<App>()) {
        auto [l, m, n]           = app->args<3>([](auto i) { return Lit::isa(i); });
        auto [T, zero, add, mul] = app->decurry()->args<4>();

        if (is_zero(zero, a) || is_zero(zero, b)) return mk_zero(zero, *l, *n);
    }

    return world.raw_app(type, callee, arg);
}

MIM_tensor_NORMALIZER_IMPL

} // namespace mim::plug::tensor
