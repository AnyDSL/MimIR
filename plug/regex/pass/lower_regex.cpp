#include "plug/regex/pass/lower_regex.h"

#include "thorin/def.h"

#include "plug/core/core.h"
#include "plug/direct/direct.h"
#include "plug/mem/mem.h"
#include "plug/regex/autogen.h"
#include "plug/regex/pass/dfa.h"
#include "plug/regex/pass/dfa2matcher.h"
#include "plug/regex/pass/dfamin.h"
#include "plug/regex/pass/nfa2dfa.h"
#include "plug/regex/pass/regex2nfa.h"
#include "plug/regex/regex.h"

namespace thorin::regex {

namespace {

Ref wrap_in_cps2ds(Ref callee) { return direct::op_cps2ds_dep(callee); }
} // namespace

Ref LowerRegex::rewrite(Ref def) {
    const Def* new_app = def;

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        if (match<regex::conj>(callee) || match<regex::disj>(callee) || match<regex::not_>(callee)
            || match<regex::range>(callee) || match<regex::any>(callee) || match<quant>(callee)) {
            const auto n = app->arg();
            auto nfa     = regex::regex2nfa(callee);
            def->world().DLOG("nfa: {}", *nfa);

            auto dfa = automaton::nfa2dfa(*nfa);
            def->world().DLOG("dfa: {}", *dfa);

            auto min_dfa = automaton::minimize_dfa(*dfa);
            new_app      = wrap_in_cps2ds(regex::dfa2matcher(def->world(), *min_dfa, n));
        }
    }

    return new_app;
}

} // namespace thorin::regex
