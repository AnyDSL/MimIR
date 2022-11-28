#include "dialects/opt/opt.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"opt", nullptr, nullptr, nullptr, [](Normalizers& normalizers) { opt::register_normalizers(normalizers); }};
}
