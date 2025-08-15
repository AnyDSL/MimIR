#include "mim/plug/tensor/tensor.h"

#include <mim/plugin.h>

#include <mim/pass/pipelinebuilder.h>

#include "mim/plug/tensor/phase/lower.h"

using namespace mim;
using namespace mim::plug;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"tensor", [](Normalizers& normalizers) { plug::tensor::register_normalizers(normalizers); },
            [](Passes& passes) { register_phase<tensor::lower_tensor, tensor::Lower>(passes); }, nullptr};
}
