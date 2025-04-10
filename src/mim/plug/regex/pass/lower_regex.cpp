#include "mim/plug/regex/pass/lower_regex.h"

#include <automaton/dfa.h>
#include <automaton/dfamin.h>
#include <automaton/nfa2dfa.h>

#include <mim/def.h>

#include <mim/plug/core/core.h>
#include <mim/plug/direct/direct.h>
#include <mim/plug/mem/mem.h>

#include "mim/plug/regex/autogen.h"
#include "mim/plug/regex/dfa2matcher.h"
#include "mim/plug/regex/regex.h"
#include "mim/plug/regex/regex2nfa.h"

namespace mim::plug::regex {

namespace {
const Def* wrap_in_cps2ds(const Def* callee) { return direct::op_cps2ds_dep(callee); }
} // namespace

const Def* LowerRegex::rewrite(const Def* def) {
    const Def* new_app = def;

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        if (match<regex::conj>(callee) || match<regex::disj>(callee) || match<regex::not_>(callee)
            || match<regex::range>(callee) || match<regex::any>(callee) || match<quant>(callee)) {
            const auto n = app->arg();
            auto nfa     = regex2nfa(callee);
            world().DLOG("nfa: {}", *nfa);

            auto dfa = automaton::nfa2dfa(*nfa);
            world().DLOG("dfa: {}", *dfa);

            auto min_dfa = automaton::minimize_dfa(*dfa);
            new_app      = wrap_in_cps2ds(dfa2matcher(world(), *min_dfa, n));
        }
    }

    return new_app;
}

} // namespace mim::plug::regex
