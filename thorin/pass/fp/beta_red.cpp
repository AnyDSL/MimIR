#include "thorin/pass/fp/beta_red.h"

#include "thorin/rewrite.h"

namespace thorin {

const Def* BetaRed::rewrite(const Def* def) {
    if (auto [app, lam] = isa_apped_nom_lam(def); isa_workable(lam) && !keep_.contains(lam)) {
        if (auto [_, ins] = data().emplace(lam); ins) {
            world().DLOG("beta-reduction {}", lam);
            return lam->reduce(app->arg()).back();
        } else {
            return proxy(app->type(), {lam, app->arg()}, 0);
        }
    }

    return def;
}

undo_t BetaRed::analyze(const Proxy* proxy) {
    auto lam = proxy->op(0)->as_nom<Lam>();
    if (keep_.emplace(lam).second) {
        world().DLOG("found proxy app of '{}' within '{}'", lam, curr_nom());
        return undo_visit(lam);
    }

    return No_Undo;
}

undo_t BetaRed::analyze(const Def* def) {
    auto undo = No_Undo;
    for (auto op : def->ops()) {
        if (auto lam = isa_workable(op->isa_nom<Lam>()); lam && keep_.emplace(lam).second) {
            auto [_, ins] = data().emplace(lam);
            if (!ins) {
                world().DLOG("non-callee-position of '{}'; undo inlining of {} within {}", lam, lam, curr_nom());
                undo = std::min(undo, undo_visit(lam));
            }
        }
    }

    return undo;
}

} // namespace thorin
