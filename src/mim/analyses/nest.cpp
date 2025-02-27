#include "mim/analyses/nest.h"

#include "mim/world.h"

#include "mim/phase/phase.h"

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
    for (auto mut : closed_muts(world)) make_node(mut, root_);
    populate();
}

void Nest::populate() {
    std::queue<Def*> queue;

    if (auto mut = root()->mut())
        queue.push(mut);
    else
        for (auto [mut, _] : root()->children()) queue.push(mut);

    while (!queue.empty()) {
        auto curr_mut  = pop(queue);
        auto curr_node = nodes_.find(curr_mut)->second.get();
        for (auto op : curr_mut->deps()) {
            for (auto local_mut : op->local_muts()) {
                auto local_node = mut2node(local_mut);
                for (auto n = curr_node; n && n->mut(); n = n->parent()) {
                    if (!local_node) {
                        if (auto var = n->mut()->has_var()) {
                            if (local_mut->free_vars().contains(var)) {
                                local_node = make_node(local_mut, n);
                                queue.push(local_mut);
                            } else {
                                continue;
                            }
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }

                    if (n->parent() == local_node->parent()) {
                        n->depends(local_node);
                        break;
                    }
                }
            }
        }
    }

    for (const auto& [_, node] : nodes_) node->tarjan();
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

bool Nest::is_recursive() const {
    for (const auto& [mut, _] : nodes()) {
        for (auto op : mut->deps()) {
            for (auto mut : op->local_muts())
                if (mut == root()->mut()) return true;
        }
    }
    return false;
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

void Nest::Node::tarjan() const {
    Stack stack;
    for (int i = 0; const auto& [_, node] : children())
        if (!node->impl_.visited) i = node->dfs(i, this, stack);
}

int Nest::Node::dfs(int i, const Node* parent, Stack& stack) const {
    this->impl_.idx = this->impl_.low = i++;
    this->impl_.visited               = true;
    this->impl_.on_stack              = true;
    stack.emplace(this);

    for (auto dep : this->depends()) {
        if (!dep->impl_.visited) i = dep->dfs(i, parent, stack);
        if (dep->impl_.on_stack) this->impl_.low = std::min(this->impl_.low, dep->impl_.low);
    }

    if (this->impl_.idx == this->impl_.low) {
        Vector<const Node*> scc;
        const Node* node;
        int num = 0;
        do {
            node                 = pop(stack);
            node->impl_.on_stack = false;
            node->impl_.low      = this->impl_.idx;
            scc.emplace_back(node);
            ++num;
        } while (node != this);

        if (num == 1 && !this->depends().contains(this)) {
            this->impl_.rec = false;
            parent_->topo_.emplace_back(this);
        } else {
            parent_->topo_.emplace_back(scc);
        }
    }

    return i;
}

} // namespace mim
