#include "plug/math/math.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"math", [](Normalizers& normalizers) { math::register_normalizers(normalizers); }, nullptr, nullptr};
}
