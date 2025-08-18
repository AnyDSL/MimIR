#include "mim/plug/tensor/phase/lower.h"

#include <mim/plug/affine/affine.h>
#include <mim/plug/core/core.h>
#include <mim/plug/direct/direct.h>

#include "mim/plug/tensor/phase/lower.h"

namespace mim::plug::tensor {

const Def* Lower::rewrite_imm(const Def* def) {
    if (auto mr = Axm::isa<map_reduce>(def))
        if (auto res = lower_map_reduce(mr)) return res;
    return def;
}

const Def* Lower::lower_map_reduce(const App*) {
    return {}; // TODO
}

} // namespace mim::plug::tensor
