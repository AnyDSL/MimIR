#include "dialects/demo/demo.h"

#include <thorin/plugin.h>
#include <thorin/pass/pass.h>

using namespace thorin;

/// heart of the dialect
/// registers passes in the different optimization phases
/// as well as normalizers for the axioms
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"demo", nullptr, nullptr, [](Normalizers& normalizers) { demo::register_normalizers(normalizers); }};
}
