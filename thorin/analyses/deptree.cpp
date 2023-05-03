#include "thorin/analyses/deptree.h"

#include "thorin/world.h"

namespace thorin {

namespace {
void merge(VarSet& vars, VarSet&& other) { vars.insert(other.begin(), other.end()); }
}

void DepTree::run() {
    for (const auto& [_, mut] : world().externals()) run(mut);
    adjust_depth(root_.get(), 0);
}

VarSet DepTree::run(Def* mut) {
    auto [i, inserted] = mut2node_.emplace(mut, std::unique_ptr<DepNode>());
    if (!inserted) {
        if (auto i = def2vars_.find(mut); i != def2vars_.end()) return i->second;
        return {};
    }

    i->second = std::make_unique<DepNode>(mut, stack_.size() + 1);
    auto node = i->second.get();
    stack_.push_back(node);

    auto result = run(mut, mut);
    auto parent = root_.get();
    for (auto var : result) {
        auto n = mut2node_[var->mut()].get();
        if (!n) {
            world().ELOG("var {} used before mut {} discovered, old var still around?", var, var->mut());
            world().ELOG("var {} : {} [{}]", var, var->type(), var->node_name());
            world().ELOG("var mut {} : {}", var->mut(), var->mut()->type());
        }
        assert(n && "Old var still around?");
        parent = n->depth() > parent->depth() ? n : parent;
    }
    if (mut->is_external() && parent != root_.get()) {
        world().WLOG("external {} would be hidden inside parent {}.", mut, parent->mut());
        node->set_parent(root_.get());
    } else
        node->set_parent(parent);

    stack_.pop_back();
    return result;
}

VarSet DepTree::run(Def* curr_mut, const Def* def) {
    if (def->dep_const()) return {};
    if (auto i = def2vars_.find(def); i != def2vars_.end()) return i->second;
    if (auto mut = def->isa_mut(); mut && curr_mut != mut) return run(mut);

    VarSet result;
    if (auto var = def->isa<Var>()) {
        result.emplace(var);
    } else {
        for (auto op : def->extended_ops()) merge(result, run(curr_mut, op));

        if (auto v = curr_mut->var()) {
            if (auto var = v->isa<Var>(); var && curr_mut == def) result.erase(var);
        }
    }

    return def2vars_[def] = result;
}

void DepTree::adjust_depth(DepNode* node, size_t depth) {
    node->depth_ = depth;

    for (const auto& child : node->children()) adjust_depth(child, depth + 1);
}

bool DepTree::depends(Def* a, Def* b) const {
    auto n = mut2node(a);
    auto m = mut2node(b);

    if (n->depth() < m->depth()) return false;

    auto i = n;
    for (size_t e = m->depth(); i->depth() != e; i = i->parent()) {}

    return i == m;
}

} // namespace thorin
