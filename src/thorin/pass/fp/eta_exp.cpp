#include "thorin/pass/fp/eta_exp.h"

#include "thorin/pass/fp/eta_red.h"

namespace thorin {

const Proxy* EtaExp::proxy(Lam* lam) { return FPPass<EtaExp, Lam>::proxy(lam->type(), {lam}, 0); }

Lam* EtaExp::new2old(Lam* new_lam) {
    if (auto old_lam = new2old_.lookup(new_lam)) {
        auto root = new2old(*old_lam); // path compression
        assert(root != new_lam);
        new2old_[new_lam] = root;
        return root;
    }

    return new_lam;
}

const Def* EtaExp::rewrite(const Def* def) {
    if (std::ranges::none_of(def->ops(), [](const Def* def) { return def->isa<Lam>(); })) return def;
    if (auto new_def = data<0>().lookup(def)) return *new_def;

    auto& [_, new_ops] = *def2new_ops_.emplace(def, def->ops()).first;

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>(); lam && lam->is_set()) {
            if (isa_callee(def, i)) {
                if (auto orig = wrap2orig_.lookup(lam)) new_ops[i] = *orig;
            } else {
                if (expand_.contains(lam)) {
                    if (new_ops[i] == lam) new_ops[i] = eta_wrap(lam);
                } else if (auto orig = wrap2orig_.lookup(lam)) {
                    if (new_ops[i] == lam) new_ops[i] = eta_wrap(*orig);
                }
            }
        }
    }

    auto new_def              = def->rebuild(world(), def->type(), new_ops, def->dbg());
    return data<0>()[new_def] = new_def;
}

Lam* EtaExp::eta_wrap(Lam* lam) {
    auto wrap = lam->stub(world(), lam->type(), lam->dbg());
    wrap2orig_.emplace(wrap, lam);
    wrap->set_name(std::string("eta_") + lam->name());
    wrap->app(false, lam, wrap->var());
    if (eta_red_) eta_red_->mark_irreducible(wrap);
    return wrap;
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
            if (expand_.contains(lam) || wrap2orig_.contains(lam)) continue;

            if (isa_callee(def, i)) {
                auto [_, l] = *data<1>().emplace(lam, Lattice::Callee).first;
                if (l == Lattice::Non_Callee_1) {
                    world().DLOG("Callee: Callee -> Expand: '{}'", lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                } else {
                    world().DLOG("Callee: Bot/Callee -> Callee: '{}'", lam);
                }
            } else {
                auto [it, first] = data<1>().emplace(lam, Lattice::Non_Callee_1);

                if (first) {
                    world().DLOG("Non_Callee: Bot -> Non_Callee_1: '{}'", lam);
                } else {
                    world().DLOG("Non_Callee: {} -> Expand: '{}'", lattice2str(it->second), lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                }
            }
        }
    }

    return undo;
}

} // namespace thorin
