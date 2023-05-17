#include "dialects/regex/regex.h"

#include <thorin/pass/pass.h>
#include <thorin/plugin.h>

#include <thorin/pass/pipelinebuilder.h>

#include "dialects/regex/passes/lower_regex.h"

using namespace thorin;

/// Heart of this Plugin.
/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"regex", [](Normalizers& normalizers) { regex::register_normalizers(normalizers); },
            [](Passes& passes) { register_pass<regex::lower_regex, regex::LowerRegex>(passes); }, nullptr};
}
