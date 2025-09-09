#include "mim/plug/tensor/tensor.h"

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"tensor", plug::tensor::register_normalizers, nullptr}; }
