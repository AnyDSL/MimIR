#include "mim/plug/tensor/phase/lower.h"

#include <mim/plug/affine/affine.h>
#include <mim/plug/core/core.h>
#include <mim/plug/direct/direct.h>
#include <mim/plug/tensor/tensor.h>

#include "mim/plug/tensor/phase/lower.h"

namespace mim::plug::tensor {

const Def* Lower::rewrite_imm_App(const App* app) {
    if (auto mr = Axm::isa<map_reduce>(app)) {
        // if (auto res = lower_map_reduce(mr)) return res;
        // TODO
    }
    return Rewriter::rewrite_imm_App(app);
}

} // namespace mim::plug::tensor
