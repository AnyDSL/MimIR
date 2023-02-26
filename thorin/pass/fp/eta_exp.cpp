#include "thorin/pass/fp/eta_exp.h"

#include "thorin/pass/fp/eta_red.h"

using namespace std::literals;

namespace thorin {

Lam* EtaExp::new2old(Lam* new_lam) {
    if (auto i = new2old_.find(new_lam); i != new2old_.end()) {
        auto root = new2old(i->second); // path compression
        assert(root != new_lam);
        new2old_[new_lam] = root;
        return root;
    }

    return new_lam;
}

const Def* EtaExp::rewrite(const Def* def) {
    if (std::ranges::none_of(def->ops(), [](const Def* def) { return def->isa<Lam>(); })) return def;
    if (auto i = old2new().find(def); i != old2new().end()) return i->second;

    auto& [_, new_ops] = *def2new_ops_.emplace(def, def->ops()).first;

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>(); lam && lam->is_set()) {
            if (isa_callee(def, i)) {
                if (auto it = exp2orig_.find(lam); it != exp2orig_.end()) new_ops[i] = it->second;
            } else {
                if (expand_.contains(lam)) {
                    if (new_ops[i] == lam) new_ops[i] = eta_exp(lam);
                } else if (auto it = exp2orig_.find(lam); it != exp2orig_.end()) {
                    if (new_ops[i] == lam) new_ops[i] = eta_exp(it->second);
                }
            }
        }
    }

    auto new_def              = def->rebuild(world(), def->type(), new_ops);
    return old2new()[new_def] = new_def;
}

Lam* EtaExp::eta_exp(Lam* lam) {
    auto exp = lam->stub(world(), lam->type());
    exp2orig_.emplace(exp, lam);
    exp->debug_suffix("eta_"s + lam->name().str());
    exp->app(false, lam, exp->var());
    if (eta_red_) eta_red_->mark_irreducible(exp);
    return exp;
}

undo_t EtaExp::analyze(const Proxy* proxy) {
    world().DLOG("found proxy: {}", proxy);
    auto lam = proxy->op(0)->as_nom<Lam>();
    if (expand_.emplace(lam).second) return undo_visit(lam);
    return No_Undo;
}

undo_t EtaExp::analyze(const Def* def) {
    auto undo = No_Undo;
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>(); lam && lam->is_set()) {
            lam = new2old(lam);
            if (expand_.contains(lam) || exp2orig_.contains(lam)) continue;

            if (isa_callee(def, i)) {
                if (auto [_, p] = *pos().emplace(lam, Pos::Callee).first; p == Pos::Non_Callee_1) {
                    world().DLOG("Callee: Callee -> Expand: '{}'", lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                } else {
                    world().DLOG("Callee: Bot/Callee -> Callee: '{}'", lam);
                }
            } else {
                auto [it, first] = pos().emplace(lam, Pos::Non_Callee_1);

                if (first) {
                    world().DLOG("Non_Callee: Bot -> Non_Callee_1: '{}'", lam);
                } else {
                    world().DLOG("Non_Callee: {} -> Expand: '{}'", pos2str(it->second), lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                }
            }
        }
    }

    return undo;
}

} // namespace thorin
