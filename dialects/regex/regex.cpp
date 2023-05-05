#include "dialects/regex/regex.h"

#include <thorin/pass/pass.h>
#include <thorin/plugin.h>

using namespace thorin;

/// Heart of this Plugin.
/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"regex", [](Normalizers& normalizers) { regex::register_normalizers(normalizers); }, nullptr, nullptr};
}
