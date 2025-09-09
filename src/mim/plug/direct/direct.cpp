#include "mim/plug/direct/direct.h"

#include <mim/plugin.h>

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"direct", direct::register_normalizers, nullptr}; }
