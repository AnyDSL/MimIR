#include "mim/plug/compile/compile.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"compile", compile::register_normalizers, nullptr}; }
