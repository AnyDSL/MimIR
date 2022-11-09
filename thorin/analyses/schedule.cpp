#include "thorin/analyses/schedule.h"

#include <queue>

#include "thorin/world.h"

#include "thorin/analyses/cfg.h"
#include "thorin/analyses/domtree.h"
#include "thorin/analyses/looptree.h"
#include "thorin/analyses/scope.h"

namespace thorin {

Scheduler::Scheduler(const Scope& s)
    : scope_(&s)
    , cfg_(&scope().f_cfg())
    , domtree_(&cfg().domtree()) {
    std::queue<const Def*> queue;
    DefSet done;

    auto enqueue = [&](const Def* def, size_t i, const Def* op) {
        if (scope().bound(op)) {
            auto [_, ins] = def2uses_[op].emplace(def, i);
            assert_unused(ins);
            if (auto [_, ins] = done.emplace(op); ins) queue.push(op);
        }
    };

    for (auto n : cfg().reverse_post_order()) {
        queue.push(n->nom());
        auto p = done.emplace(n->nom());
        assert_unused(p.second);
    }

    while (!queue.empty()) {
        auto def = queue.front();
        queue.pop();

        if (!def->is_set()) continue;

        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            // all reachable noms have already been registered above
            // NOTE we might still see references to unreachable noms in the schedule
            if (!def->op(i)->isa_nom()) enqueue(def, i, def->op(i));
        }

        if (!def->type()->isa_nom()) enqueue(def, -1, def->type());
    }
}

Def* Scheduler::early(const Def* def) {
    if (auto i = early_.find(def); i != early_.end()) return i->second;
    if (def->dep_const() || !scope().bound(def)) return early_[def] = scope().entry();
    if (auto var = def->isa<Var>()) return early_[def] = var->nom();

    auto result = scope().entry();
    for (auto op : def->extended_ops()) {
        if (!op->isa_nom() && def2uses_.find(op) != def2uses_.end()) {
            auto nom = early(op);
            if (domtree().depth(cfg(nom)) > domtree().depth(cfg(result))) result = nom;
        }
    }

    return early_[def] = result;
}

Def* Scheduler::late(const Def* def) {
    if (auto i = late_.find(def); i != late_.end()) return i->second;
    if (def->dep_const() || !scope().bound(def)) return early_[def] = scope().entry();

    Def* result = nullptr;
    if (auto nom = def->isa_nom()) {
        result = nom;
    } else if (auto var = def->isa<Var>()) {
        result = var->nom();
    } else {
        for (auto use : uses(def)) {
            auto nom = late(use);
            result   = result ? domtree().least_common_ancestor(cfg(result), cfg(nom))->nom() : nom;
        }
    }

    return late_[def] = result;
}

Def* Scheduler::smart(const Def* def) {
    if (auto i = smart_.find(def); i != smart_.end()) return i->second;

    auto e = cfg(early(def));
    auto l = cfg(late(def));
    auto s = l;

    int depth = cfg().looptree()[l]->depth();
    for (auto i = l; i != e;) {
        auto idom = domtree().idom(i);
        assert(i != idom);
        i = idom;

        if (i == nullptr) {
            scope_->world().WLOG("this should never occur - don't know where to put {}", def);
            s = l;
            break;
        }

        if (int cur_depth = cfg().looptree()[i]->depth(); cur_depth < depth) {
            s     = i;
            depth = cur_depth;
        }
    }

    return smart_[def] = s->nom();
}

Schedule schedule(const Scope& scope) {
    // until we have sth better simply use the RPO of the CFG
    Schedule result;
    for (auto n : scope.f_cfg().reverse_post_order()) result.emplace_back(n->nom());

    return result;
}

} // namespace thorin
