#include "dialects/regex/passes/lower_regex.h"

#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"
#include "dialects/regex/regex.h"

namespace thorin::regex {
namespace {
Ref rewrite_arg(Ref ref, Ref n);

Ref wrap_in_cps2ds(Ref callee) { return direct::op_cps2ds_dep(callee); }

Ref cls_impl(Match<regex::cls, Axiom> cls_ax) {
    auto& world = cls_ax->world();
    switch (cls_ax.id()) {
        case cls::d: return world.annex<regex::match_d>();
        case cls::D: return world.annex<regex::match_D>();
        case cls::w: return world.annex<regex::match_w>();
        case cls::W: return world.annex<regex::match_W>();
        case cls::s: return world.annex<regex::match_s>();
        case cls::S: return world.annex<regex::match_S>();
        case cls::any: return world.annex<regex::match_any>();
    }
    return cls_ax.axiom();
}

Ref lit_impl(Match<regex::lit, App> lit_app) {
    auto& world = lit_app->world();
    return world.app(world.annex<regex::match_lit>(), lit_app->arg());
}

Ref range_impl(Match<regex::range, App> range_app) {
    auto& world = range_app->world();
    return world.app(world.annex<regex::match_range>(), range_app->arg());
}

Ref not_impl(Match<regex::not_, App> not_app) {
    auto& world = not_app->world();
    return world.annex<regex::match_not>();
}

Ref quant_impl(Match<regex::quant, App> quant_app) {
    auto& world = quant_app->world();
    switch (quant_app.id()) {
        case quant::zeroOrOne: return world.annex<regex::match_zeroOrOne>();
        case quant::zeroOrMore: return world.annex<regex::match_zeroOrMore>();
        case quant::oneOrMore: return world.annex<regex::match_oneOrMore>();
    }
    return quant_app.axiom();
}

Ref conj_impl(Match<regex::conj, App> conj_app) {
    auto& world = conj_app->world();
    return world.annex<regex::match_conj>();
}

Ref disj_impl(Match<regex::disj, App> disj_app) {
    auto& world = disj_app->world();
    return world.annex<regex::match_disj>();
}

Ref rewrite_args(Ref arg, Ref n) {
    if (arg->as_lit_arity() > 1) {
        auto args = arg->projs();
        std::vector<const Def*> newArgs;
        newArgs.reserve(arg->as_lit_arity());
        for (auto sub_arg : args) newArgs.push_back(rewrite_arg(sub_arg, n));
        return arg->world().tuple(newArgs);
    } else {
        return rewrite_arg(arg, n);
    }
}

Ref rewrite_arg(Ref def, Ref n) {
    auto& world        = def->world();
    const Def* new_app = def;

    if (auto cls_ax = thorin::match<cls>(def)) new_app = world.app(cls_impl(cls_ax), n);
    if (auto lit_app = thorin::match<lit>(def)) new_app = world.app(lit_impl(lit_app), n);
    if (auto range_app = thorin::match<range>(def)) new_app = world.app(range_impl(range_app), n);
    if (auto not_app = thorin::match<not_>(def))
        new_app = world.iapp(world.app(not_impl(not_app), n), rewrite_args(not_app->arg(), n));
    if (auto conj_app = thorin::match<conj>(def))
        new_app = world.iapp(world.app(conj_impl(conj_app), n), rewrite_args(conj_app->arg(), n));
    if (auto disj_app = thorin::match<disj>(def))
        new_app = world.iapp(world.app(disj_impl(disj_app), n), rewrite_args(disj_app->arg(), n));
    if (auto quant_app = thorin::match<quant>(def))
        new_app = world.iapp(world.app(quant_impl(quant_app), n), rewrite_args(quant_app->arg(), n));
    return new_app;
}

} // namespace

Ref LowerRegex::rewrite(Ref def) {
    auto& world        = def->world();
    const Def* new_app = def;

    if (auto app = def->isa<App>()) {
        const auto n = app->arg();
        // clang-format off
        if (auto cls_ax = thorin::match<cls>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(cls_impl(cls_ax), n));
        if (auto lit_app = thorin::match<lit>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(lit_impl(lit_app), n));
        if (auto range_app = thorin::match<range>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(range_impl(range_app), n));
        if (auto not_app = thorin::match<not_>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(world.iapp(not_impl(not_app), n), rewrite_args(not_app->arg(), n)));
        if (auto conj_app = thorin::match<conj>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(world.iapp(conj_impl(conj_app), n), rewrite_args(conj_app->arg(), n)));
        if (auto disj_app = thorin::match<disj>(app->callee()))
            new_app = wrap_in_cps2ds(world.app(world.iapp(disj_impl(disj_app), n), rewrite_args(disj_app->arg(), n)));
        if (auto quant_app = thorin::match<quant>(app->callee())) {
            new_app = wrap_in_cps2ds(world.app(world.iapp(quant_impl(quant_app), n), rewrite_args(quant_app->arg(), n)));
        }
        // clang-format on
    }

    return new_app;
}

} // namespace thorin::regex
