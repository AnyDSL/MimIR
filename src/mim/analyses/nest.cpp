#include "mim/analyses/nest.h"

#include "mim/world.h"

namespace mim {

Nest::Nest(Def* r)
    : world_(r->world())
    , root_(make_node(r)) {
    populate();
}

Nest::Nest(View<Def*> muts)
    : world_(muts.front()->world())
    , root_(make_node(nullptr)) {
    for (auto mut : muts) make_node(mut, root_);
    populate();
}

Nest::Nest(World& world)
    : world_(world)
    , root_(make_node(nullptr)) {
    for (auto [_, mut] : world.externals()) make_node(mut, root_);
    populate();
}

void Nest::populate() {
    std::queue<Def*> queue;

    if (auto mut = root()->mut())
        queue.push(mut);
    else
        for (auto [mut, _] : root()->children()) queue.push(mut);

    while (!queue.empty()) {
        auto curr = pop(queue);
        auto node = nodes_.find(curr)->second.get();
        for (auto op : curr->deps()) {
            for (auto mut : op->local_muts()) {
                if (!mut2node(mut)) {
                    if (find_parent(mut, node)) queue.push(mut);
                }
            }
        }
    }
}

Nest::Node* Nest::make_node(Def* mut, Node* parent) {
    auto node = std::make_unique<Node>(mut, parent);
    auto res  = node.get();
    nodes_.emplace(mut, std::move(node));
    return res;
}

/// Tries to place @p mut as high as possible.
Nest::Node* Nest::find_parent(Def* mut, Node* begin) {
    for (Node* node = begin; node && node->mut(); node = node->parent()) {
        if (auto var = node->mut()->has_var()) {
            if (mut->free_vars().contains(var)) return make_node(mut, node);
        }
    }

    return nullptr;
}

bool Nest::is_recursive() const {
    for (const auto& [mut, _] : nodes()) {
        for (auto op : mut->deps()) {
            for (auto mut : op->local_muts())
                if (mut == root()->mut()) return true;
        }
    }
    return false;
}

Vars Nest::vars() const {
    if (!vars_) {
        auto vec = Vector<const Var*>();
        vec.reserve(num_nodes());
        for (const auto& [mut, _] : nodes_) {
            if (mut) {
                if (auto var = mut->has_var()) vec.emplace_back(var);
            }
        }
        vars_ = world().vars().create(vec.begin(), vec.end());
    }

    return vars_;
}

const Nest::Node* Nest::lca(const Node* n, const Node* m) {
    while (n->depth() < m->depth()) m = m->parent();
    while (m->depth() < n->depth()) n = n->parent();
    while (n != m) {
        n = n->parent();
        m = m->parent();
    }
    return n;
}

} // namespace mim
