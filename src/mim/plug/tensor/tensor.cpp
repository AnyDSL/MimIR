#include "mim/plug/tensor/tensor.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"tensor", plug::tensor::register_normalizers, nullptr, nullptr};
}
