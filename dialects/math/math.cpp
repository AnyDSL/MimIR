#include "dialects/math/math.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"math", nullptr, [](Backends&) {},
            [](Normalizers& normalizers) { math::register_normalizers(normalizers); }};
}
