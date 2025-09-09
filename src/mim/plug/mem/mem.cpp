#include "mim/plug/mem/mem.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"mem", mem::register_normalizers, nullptr}; }
