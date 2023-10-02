#include "dialects/regex/pass/lower_regex.h"

#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"
#include "dialects/regex/regex.h"

namespace thorin::regex {
namespace {
Ref rewrite_arg(Ref ref, Ref n);

Ref wrap_in_cps2ds(Ref callee) { return direct::op_cps2ds_dep(callee); }

Ref rewrite_args(Ref arg, Ref n) {
    size_t num = arg->num_projs();
    DefArray new_args(num, [=](size_t i) { return rewrite_arg(arg->proj(num, i), n); });
    return arg->world().tuple(new_args);
}

Ref rewrite_arg(Ref def, Ref n) {
    auto& world        = def->world();
    const Def* new_app = def;

    // clang-format off
    if (auto any   = thorin::match<regex::any  >(   def)) new_app = world.call<regex::match_any     >(n);
    if (auto range = thorin::match<regex::range>(   def)) new_app = world.call<regex::match_range   >(range->arg(), n);
    if (auto not_  = thorin::match<regex::not_ >(   def)) new_app = world.call<regex::match_not     >(n, rewrite_args(not_->arg(), n));
    if (auto conj  = thorin::match<regex::conj >(   def)) new_app = world.call<regex::match_conj    >(n, rewrite_args(conj->arg(), n));
    if (auto disj  = thorin::match<regex::disj >(   def)) new_app = world.call<regex::match_disj    >(n, rewrite_args(disj->arg(), n));
    if (auto opt   = thorin::match(quant::optional, def)) new_app = world.call<regex::match_optional>(n, rewrite_args(opt ->arg(), n));
    if (auto star  = thorin::match(quant::star,     def)) new_app = world.call<regex::match_star    >(n, rewrite_args(star->arg(), n));
    if (auto plus  = thorin::match(quant::plus,     def)) new_app = world.call<regex::match_plus    >(n, rewrite_args(plus->arg(), n));
    // clang-format on
    return new_app;
}

} // namespace

Ref LowerRegex::rewrite(Ref def) {
    const Def* new_app = def;

    if (auto app = def->isa<App>()) {
        const auto n = app->arg();
        // clang-format off
        if (auto any   = thorin::match<regex::any  >(   app->callee())) new_app = wrap_in_cps2ds(world().call<match_any     >(n));
        if (auto range = thorin::match<regex::range>(   app->callee())) new_app = wrap_in_cps2ds(world().call<match_range   >(range->arg(), n));
        if (auto not_  = thorin::match<regex::not_ >(   app->callee())) new_app = wrap_in_cps2ds(world().call<match_not     >(n, rewrite_args(not_->arg(), n)));
        if (auto conj  = thorin::match<regex::conj >(   app->callee())) new_app = wrap_in_cps2ds(world().call<match_conj    >(n, rewrite_args(conj->arg(), n)));
        if (auto disj  = thorin::match<regex::disj >(   app->callee())) new_app = wrap_in_cps2ds(world().call<match_disj    >(n, rewrite_args(disj->arg(), n)));
        if (auto opt   = thorin::match(quant::optional, app->callee())) new_app = wrap_in_cps2ds(world().call<match_optional>(n, rewrite_args(opt ->arg(), n)));
        if (auto star  = thorin::match(quant::star,     app->callee())) new_app = wrap_in_cps2ds(world().call<match_star    >(n, rewrite_args(star->arg(), n)));
        if (auto plus  = thorin::match(quant::plus,     app->callee())) new_app = wrap_in_cps2ds(world().call<match_plus    >(n, rewrite_args(plus->arg(), n)));
        // clang-format on
    }

    return new_app;
}

} // namespace thorin::regex
