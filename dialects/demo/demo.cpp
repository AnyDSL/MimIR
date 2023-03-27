#include "dialects/demo/demo.h"

#include <thorin/plugin.h>
#include <thorin/pass/pass.h>

using namespace thorin;

/// Heart of this Plugin.
/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"demo", nullptr, nullptr, [](Normalizers& normalizers) { demo::register_normalizers(normalizers); }};
}
