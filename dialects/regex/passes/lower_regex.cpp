#include "dialects/regex/passes/lower_regex.h"

#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"
#include "dialects/regex/regex.h"

namespace thorin::regex {
namespace {
Ref dispatch_rewrite(Ref ref);

Ref wrap_in_cps2ds(Ref callee) {
    return direct::op_cps2ds_dep(callee);
}

Ref cls_impl(const Axiom* cls_ax) {
    auto& world = cls_ax->world();
    switch (static_cast<cls>(cls_ax->flags())) {
        case cls::d: return world.annex<regex::match_d>();
        case cls::D: return world.annex<regex::match_D>();
        case cls::w: return world.annex<regex::match_w>();
        case cls::W: return world.annex<regex::match_W>();
        case cls::s: return world.annex<regex::match_s>();
        case cls::S: return world.annex<regex::match_S>();
        case cls::any: return world.annex<regex::match_any>();
    }
    return cls_ax;
    // return cls_app.axiom();
}

Ref cls_impl(Match<regex::cls, App> cls_app) {
    auto& world = cls_app->world();
    switch (cls_app.id()) {
        case cls::d: return world.annex<regex::match_d>();
        case cls::D: return world.annex<regex::match_D>();
        case cls::w: return world.annex<regex::match_w>();
        case cls::W: return world.annex<regex::match_W>();
        case cls::s: return world.annex<regex::match_s>();
        case cls::S: return world.annex<regex::match_S>();
        case cls::any: return world.annex<regex::match_any>();
    }
    return cls_app->callee();
    // return cls_app.axiom();
}

Ref rewrite_args(Ref arg) {
    if (arg->as_lit_arity() > 1) {
        auto args = arg->projs();
        std::vector<const Def*> newArgs;
        newArgs.reserve(arg->as_lit_arity());
        for (auto sub_arg : args) newArgs.push_back(dispatch_rewrite(sub_arg));
        return arg->world().tuple(newArgs);
    } else {
        return dispatch_rewrite(arg);
    }
}

Ref conj_impl(Match<regex::conj, App> conj_app) {
    auto& world = conj_app->world();
    world.DLOG("conj!! {}, {}, {}", conj_app, conj_app->callee(), conj_app->callee()->as<App>()->callee());

    return world.app(world.annex<regex::match_conj>(), conj_app->callee()->as<App>()->arg());
    // return conj_app->callee();
}

Ref dispatch_rewrite(Ref def) {
    auto& world        = def->world();
    const Def* new_app = def;

    if (auto ax = def->isa<Axiom>(); ax && (ax->flags() & Annex::Base<cls>) == Annex::Base<cls>) new_app = cls_impl(ax);
    // if (auto cls_app = thorin::match<cls>(def)) new_app = world.iapp(cls_impl(cls_app), cls_app->arg());
    if (auto conj_app = thorin::match<conj>(def))
        new_app = world.iapp(conj_impl(conj_app), rewrite_args(conj_app->arg()));

    // if (new_app != def) {
    //     new_app->dump(0);
    //     world.dump();
    // }

    return new_app;
}
} // namespace

Ref LowerRegex::rewrite(Ref def) {
    auto& world        = def->world();
    const Def* new_app = def;

    // if (auto cls_app = thorin::match<cls>(def)) new_app = cls_impl(cls_app);
    if (auto cls_app = thorin::match<cls>(def))
        new_app = world.app(wrap_in_cps2ds(world.app(cls_impl(cls_app), cls_app->callee()->as<App>()->arg())),
                            cls_app->arg());
    if (auto app = def->isa<App>()) {
        if (auto conj_app = thorin::match<conj>(app->callee())) {
            new_app = wrap_in_cps2ds(world.app(world.iapp(conj_impl(conj_app), rewrite_args(conj_app->arg())), app->arg()));
        }
    }
    // if (new_app != def) {
    //     new_app->dump(0);
    //     world.dump();
    // }

    return new_app;
}

} // namespace thorin::regex
