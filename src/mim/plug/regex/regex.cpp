#include "mim/plug/regex/regex.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

#include "mim/plug/regex/pass/lower_regex.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Phases&, Passes& passes) { PassMan::hook<regex::lower_regex, regex::LowerRegex>(passes); }

/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axm%s.
extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"regex", [](Normalizers& n) { regex::register_normalizers(n); }, reg_stages, nullptr};
}
