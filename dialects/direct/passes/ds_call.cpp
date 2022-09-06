#include "dialects/direct/passes/ds_call.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/direct/direct.h"
#include "dialects/direct/passes/ds2cps.h"

namespace thorin::direct {

const Def* custom_rewrite(const Def* def, Def2Def& map) {
    auto& world = def->world();
    if (map.find(def) != map.end()) {
        world.DLOG("found in map {}", def);
        auto res = map[def];
        world.DLOG("res {}", res);
        return res;
    }

    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto args   = app->args();

        DefArray new_args(args, [&](const Def* arg) { return custom_rewrite(arg, map); });

        return world.app(custom_rewrite(callee, map), new_args);
    }

    DefArray new_ops(def->ops(), [&](const Def* op) { return custom_rewrite(op, map); });

    if (def->isa<Tuple>()) return world.tuple(new_ops, def->dbg());

    return def->rebuild(world, def->type(), new_ops, def->dbg());
}

void DSCall::enter() {
    Lam* lam = curr_nom();
    if (!lam->isa_nom()) {
        lam->world().DLOG("skipped non-nom {}", lam);
        return;
    }
    // rewrite_lam(lam);

    world().DLOG("known replacements:");
    auto subst = ds2cps_->get_subst();
    for (auto const& [key, val] : subst) world().DLOG("  {} : {} => {} : {}", key, key->type(), val, val->type());
    // assert(0);

    lams.push_back(lam);

    for (auto lam : lams) {
        // Scope scope{lam};
        // ScopeRewriter rewriter(lam->body()->world(), scope);
        // for (auto const& [key, val] : subst) rewriter.old2new.insert({key, world().lit_nat(42)});
        // // rewriter.old2new.insert(subst.begin(), subst.end());
        auto body = lam->body();
        // auto b    = rewriter.rewrite(body);
        // Def2Def subst_map{};
        world().DLOG("rewrite {}", lam);
        // for (auto const& [key, val] : subst) subst_map.insert({key, world().lit_nat(42)});
        world().DLOG("built map");
        // auto b = custom_rewrite(body, subst_map);
        auto b = custom_rewrite(body, subst);

        lam->set_body(b);
        world().ILOG("rewrote {}", lam);
        world().DLOG("new body: {}", b);
        if (body != b) world().ILOG("changed body of {}", lam);
    }
}

} // namespace thorin::direct