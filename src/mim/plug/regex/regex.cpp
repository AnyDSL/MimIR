#include "mim/plug/regex/regex.h"

#include <mim/pass.h>
#include <mim/plugin.h>

#include "mim/plug/regex/pass/lower_regex.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Phases&, Flags2Passes& passes) { PassMan::hook<regex::lower_regex, regex::LowerRegex>(passes); }

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"regex", regex::register_normalizers, reg_stages, nullptr}; }
