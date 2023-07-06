#include "dialects/regex/passes/lower_regex.h"

#include "thorin/def.h"

#include "thorin/util/assert.h"

#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"
#include "dialects/regex/regex.h"

namespace thorin::regex {
namespace {
Ref rewrite_arg(Ref ref, Ref n);

Ref wrap_in_cps2ds(Ref callee) { return direct::op_cps2ds_dep(callee); }

Ref quant_impl(Match<regex::quant, App> quant_app) {
    auto& world = quant_app->world();
    switch (quant_app.id()) {
        case quant::optional: return world.annex<regex::match_optional>();
        case quant::star: return world.annex<regex::match_star>();
        case quant::plus: return world.annex<regex::match_plus>();
    }
    return quant_app.axiom();
}

Ref rewrite_args(Ref arg, Ref n) {
    if (arg->as_lit_arity() > 1) {
        auto args = arg->projs();
        std::vector<const Def*> new_args;
        new_args.reserve(arg->as_lit_arity());
        for (auto sub_arg : args) new_args.push_back(rewrite_arg(sub_arg, n));
        return arg->world().tuple(new_args);
    } else {
        return rewrite_arg(arg, n);
    }
}

Ref rewrite_arg(Ref def, Ref n) {
    auto& world        = def->world();
    const Def* new_app = def;

    // clang-format off
    if (auto _any   = thorin::match<any  >(def)) new_app = world.call<regex::match_any  >(n);
    if (auto _range = thorin::match<range>(def)) new_app = world.call<regex::match_range>(_range->arg(), n);
    if (auto _not   = thorin::match<not_ >(def)) new_app = world.call<regex::match_not  >(n, rewrite_args( _not->arg(), n));
    if (auto _conj  = thorin::match<conj >(def)) new_app = world.call<regex::match_conj >(n, rewrite_args(_conj->arg(), n));
    if (auto _disj  = thorin::match<disj >(def)) new_app = world.call<regex::match_disj >(n, rewrite_args(_disj->arg(), n));
    if (auto _quant = thorin::match<quant>(def)) new_app = world.call_(quant_impl(_quant), n, rewrite_args(_quant->arg(), n));
    // clang-format on
    return new_app;
}

} // namespace

Ref LowerRegex::rewrite(Ref def) {
    auto& world        = def->world();
    const Def* new_app = def;

    if (auto app = def->isa<App>()) {
        const auto n = app->arg();
        // clang-format off
        if (auto _any   = thorin::match<any  >(app->callee())) new_app = wrap_in_cps2ds(world.call<match_any  >(n));
        if (auto _range = thorin::match<range>(app->callee())) new_app = wrap_in_cps2ds(world.call<match_range>(_range->arg(), n));
        if (auto _not   = thorin::match<not_ >(app->callee())) new_app = wrap_in_cps2ds(world.call<match_not  >(n, rewrite_args( _not->arg(), n)));
        if (auto _conj  = thorin::match<conj >(app->callee())) new_app = wrap_in_cps2ds(world.call<match_conj >(n, rewrite_args(_conj->arg(), n)));
        if (auto _disj  = thorin::match<disj >(app->callee())) new_app = wrap_in_cps2ds(world.call<match_disj >(n, rewrite_args(_disj->arg(), n)));
        if (auto _quant = thorin::match<quant>(app->callee())) new_app = wrap_in_cps2ds(world.call_(quant_impl(_quant), n, rewrite_args(_quant->arg(), n)));
        // clang-format on
    }

    return new_app;
}

} // namespace thorin::regex
