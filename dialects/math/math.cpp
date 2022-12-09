#include "dialects/math/math.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"math", nullptr, [](Backends&) {},
            [](Normalizers& normalizers) { math::register_normalizers(normalizers); }};
}
