#include "mim/plug/regex/regex.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>
#include <mim/pass/pipelinebuilder.h>

#include "mim/plug/regex/pass/lower_regex.h"

using namespace mim;
using namespace mim::plug;

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"regex", [](Normalizers& normalizers) { regex::register_normalizers(normalizers); },
            [](Passes& passes) { register_pass<regex::lower_regex, regex::LowerRegex>(passes); }, nullptr};
}
