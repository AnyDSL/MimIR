#include "thorin/pass/fp/beta_red.h"

#include "thorin/rewrite.h"

namespace thorin {

Ref BetaRed::rewrite(Ref def) {
    if (auto [app, lam] = isa_apped_mut_lam(def); isa_workable(lam) && !keep_.contains(lam)) {
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
    auto lam = proxy->op(0)->as_mut<Lam>();
    if (keep_.emplace(lam).second) {
        world().DLOG("found proxy app of '{}' within '{}'", lam, curr_mut());
        return undo_visit(lam);
    }

    return No_Undo;
}

undo_t BetaRed::analyze(Ref def) {
    auto undo = No_Undo;
    for (auto op : def->ops()) {
        if (auto lam = isa_workable(op->isa_mut<Lam>()); lam && keep_.emplace(lam).second) {
            auto [_, ins] = data().emplace(lam);
            if (!ins) {
                world().DLOG("non-callee-position of '{}'; undo inlining of {} within {}", lam, lam, curr_mut());
                undo = std::min(undo, undo_visit(lam));
            }
        }
    }

    return undo;
}

} // namespace thorin
