#include "dialects/tool/tool.h"

#include <thorin/config.h>
#include <thorin/dialects.h>
#include <thorin/pass/pass.h>

using namespace thorin;

/// heart of the dialect
/// registers passes in the different optimization phases
/// as well as normalizers for the axioms
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"tool", nullptr, nullptr, [](Normalizers& normalizers) { tool::register_normalizers(normalizers); }};
}
