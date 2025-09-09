#include "mim/plug/refly/refly.h"

using namespace mim;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"refly", plug::refly::register_normalizers, nullptr};
}
