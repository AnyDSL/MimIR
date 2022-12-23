#include "thorin/analyses/deptree.h"

#include "thorin/world.h"

namespace thorin {

static void merge(VarSet& vars, VarSet&& other) { vars.insert(other.begin(), other.end()); }

void DepTree::run() {
    for (const auto& [_, nom] : world().externals()) run(nom);
    adjust_depth(root_.get(), 0);
}

VarSet DepTree::run(Def* nom) {
    auto [i, inserted] = nom2node_.emplace(nom, std::unique_ptr<DepNode>());
    if (!inserted) {
        if (auto i = def2vars_.find(nom); i != def2vars_.end()) return i->second;
        return {};
    }

    i->second = std::make_unique<DepNode>(nom, stack_.size() + 1);
    auto node = i->second.get();
    stack_.push_back(node);

    auto result = run(nom, nom);
    auto parent = root_.get();
    for (auto var : result) {
        auto n = nom2node_[var->nom()].get();
        if (!n) {
            world().ELOG("var {} used before nom {} discovered, old var still around?", var, var->nom());
            world().ELOG("var {} : {} [{}]", var, var->type(), var->node_name());
            world().ELOG("var nom {} : {}", var->nom(), var->nom()->type());
        }
        assert(n && "Old var still around?");
        // if (n)
        parent = n->depth() > parent->depth() ? n : parent;
    }
    if (nom->is_external() && parent != root_.get()) {
        world().WLOG("external {} would be hidden inside parent {}..", nom, parent->nom());
        node->set_parent(root_.get());
    } else
        node->set_parent(parent);

    stack_.pop_back();
    return result;
}

VarSet DepTree::run(Def* curr_nom, const Def* def) {
    if (def->dep_const()) return {};
    if (auto i = def2vars_.find(def); i != def2vars_.end()) return i->second;
    if (auto nom = def->isa_nom(); nom && curr_nom != nom) return run(nom);

    VarSet result;
    if (auto var = def->isa<Var>()) {
        result.emplace(var);
    } else {
        for (auto op : def->extended_ops()) merge(result, run(curr_nom, op));

        if (auto var = curr_nom->var()) {
            if (curr_nom == def) result.erase(var);
        }
    }

    return def2vars_[def] = result;
}

void DepTree::adjust_depth(DepNode* node, size_t depth) {
    node->depth_ = depth;

    for (const auto& child : node->children()) adjust_depth(child, depth + 1);
}

bool DepTree::depends(Def* a, Def* b) const {
    auto n = nom2node(a);
    auto m = nom2node(b);

    if (n->depth() < m->depth()) return false;

    auto i = n;
    for (size_t e = m->depth(); i->depth() != e; i = i->parent()) {}

    return i == m;
}

} // namespace thorin
