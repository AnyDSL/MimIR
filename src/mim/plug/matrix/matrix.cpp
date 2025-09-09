#include "mim/plug/matrix/matrix.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"matrix", matrix::register_normalizers, nullptr}; }
