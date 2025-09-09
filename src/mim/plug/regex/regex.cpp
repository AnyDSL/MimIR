#include "mim/plug/regex/regex.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"regex", regex::register_normalizers, nullptr}; }
