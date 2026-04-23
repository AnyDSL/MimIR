#include "mim/plug/tensor/tensor.h"

#include "mim/plugin.h"

#include "mim/plug/tensor/phase/lower.h"

using namespace mim;
using namespace mim::plug;

namespace mim::plug::tensor {
void reg_stages(Flags2Stages& stages) { Stage::hook<lower_tensor, phase::Lower>(stages); }
} // namespace mim::plug::tensor

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"tensor", tensor::register_normalizers, tensor::reg_stages, nullptr};
}
