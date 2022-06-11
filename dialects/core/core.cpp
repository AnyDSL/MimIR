#include "dialects/core.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/core/be/ll/ll.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"core", nullptr, [](Backends& backends) { backends["ll"] = &ll::emit; },
            [](Normalizers& normalizers) { core::register_normalizers(normalizers); }};
}
