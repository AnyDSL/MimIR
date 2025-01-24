#include "mim/plug/tensor/phase/matmul_phase.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

Ref MatmulPhase::rewrite_imm(Ref def) {
    if (def->isa<Axiom>()) return def;

    if (auto mm = match<matmul>(def)) {
        std::vector<nat_t> dims;

        if (auto app = mm->callee()->isa<App>()) {
            auto [a, b]              = mm->args<2>();
            auto [l, m, n]           = app->args<3>([](auto i) { return Lit::isa(i); });
            auto [T, zero, add, mul] = app->decurry()->args<4>();

            if (is_zero(zero, a) || is_zero(zero, b)) return mk_zero(zero, *l, *n);
        }
    }

    return Rewriter::rewrite_imm(def); // continue recursive rewriting with everything else
}

} // namespace mim::plug::tensor
