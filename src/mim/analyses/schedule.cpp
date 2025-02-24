#include "mim/analyses/schedule.h"

#include <memory>
#include <queue>

#include "mim/world.h"

#include "mim/analyses/cfg.h"
#include "mim/analyses/domtree.h"
#include "mim/analyses/looptree.h"

namespace mim {

Scheduler::Scheduler(const Nest& nest)
    : nest_(&nest)
    , cfg_(std::make_unique<CFG>(nest))
    , domtree_(&cfg().domtree()) {
    std::queue<const Def*> queue;
    DefSet done;

    auto enqueue = [&](const Def* def, size_t i, const Def* op) {
        if (nest.contains(op)) {
            assert_emplace(def2uses_[op], def, i);
            if (auto [_, ins] = done.emplace(op); ins) queue.push(op);
        }
    };

    for (const auto& [mut, _] : nest.nodes()) {
        queue.push(mut);
        assert_emplace(done, mut);
    }

    while (!queue.empty()) {
        auto def = queue.front();
        queue.pop();

        if (!def->is_set()) continue;

        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            // all reachable muts have already been registered above
            // NOTE we might still see references to unreachable muts in the schedule
            if (!def->op(i)->isa_mut()) enqueue(def, i, def->op(i));
        }

        if (!def->type()->isa_mut()) enqueue(def, -1, def->type());
    }
}

const Nest::Node* Scheduler::early(const Def* def) {
    if (auto i = early_.find(def); i != early_.end()) return i->second;
    if (def->has_const_dep() || !nest().contains(def)) return early_[def] = nest().root();
    if (auto var = def->isa<Var>()) return early_[def] = nest()[var->mut()];

    auto result = nest().root();
    for (auto op : def->deps()) {
        if (!op->isa_mut() && nest().contains(op)) {
            auto node = early(op);
            if (node->depth() > result->depth()) result = node;
        }
    }

    return early_[def] = result;
}

const Nest::Node* Scheduler::late(Def* curr_mut, const Def* def) {
    if (auto i = late_.find(def); i != late_.end()) return i->second;
    if (def->has_const_dep() || !nest().contains(def)) return late_[def] = nest().root();

    const Nest::Node* result = nullptr;
    if (auto mut = def->isa_mut()) {
        result = nest()[mut];
    } else if (auto var = def->isa<Var>()) {
        result = nest()[var->mut()];
    } else {
        for (auto use : uses(def)) {
            auto mut = late(curr_mut, use);
            result   = result ? Nest::lca(result, mut) : mut;
        }
    }

    if (!result) result = nest()[curr_mut];

    return late_[def] = result;
}

Def* Scheduler::smart(Def* curr_mut, const Def* def) {
    if (auto i = smart_.find(def); i != smart_.end()) return i->second;

    auto e = cfg(early(def)->mut());
    auto l = cfg(late(curr_mut, def)->mut());
    auto s = l;

    int depth = cfg().looptree()[l]->depth();
    for (auto i = l; i != e;) {
        auto idom = domtree().idom(i);
        assert(i != idom);
        i = idom;

        if (i == nullptr) {
            world().WLOG("this should never occur - don't know where to put {}", def);
            s = l;
            break;
        }

        if (int cur_depth = cfg().looptree()[i]->depth(); cur_depth < depth) {
            s     = i;
            depth = cur_depth;
        }
    }

    return smart_[def] = s->mut();
}

Scheduler::Schedule Scheduler::schedule(const CFG& cfg) {
    // until we have sth better simply use the RPO of the CFG
    Schedule result;
    for (auto n : cfg.reverse_post_order()) result.emplace_back(n->mut());

    return result;
}

} // namespace mim
