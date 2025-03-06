#include "mim/nest.h"

#include "mim/world.h"

#include "mim/phase/phase.h"

#include "fe/assert.h"

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
    std::queue<const Node*> queue;

    if (root()->mut())
        queue.push(root());
    else
        for (auto [_, child] : root()->children()) queue.push(child);

    while (!queue.empty()) {
        auto curr_node = pop(queue);
        for (auto local_mut : curr_node->mut()->mut_local_muts()) {
            if (mut2node(local_mut) || !contains(local_mut)) continue;

            if (curr_node->level() < local_mut->free_vars().size()) {
                for (auto node = curr_node;; node = node->parent()) {
                    if (auto var = node->mut()->has_var()) {
                        if (local_mut->free_vars().contains(var)) {
                            queue.push(make_node(local_mut, node));
                            break;
                        }
                    }
                }
            } else {
                uint32_t max       = 0;
                const Node* parent = root();
                for (auto var : local_mut->free_vars()) {
                    if (auto node = mut2node(var->mut()); node && node->level() > max) {
                        max    = node->level();
                        parent = node;
                    }
                }
                queue.push(make_node(local_mut, parent));
            }
        }
    }
}

Nest::Node* Nest::make_node(Def* mut, const Node* parent) {
    auto node = std::unique_ptr<Node>(new Node(*this, mut, parent)); // can't use make_unique - c'tor is private
    auto res  = node.get();
    nodes_.emplace(mut, std::move(node));
    if (mut) {
        if (auto var = mut->has_var()) vars_ = world().vars().insert(vars_, var);
    }
    return res;
}

const Nest::Node* Nest::lca(const Node* n, const Node* m) {
    while (n->level() < m->level()) m = m->parent();
    while (m->level() < n->level()) n = n->parent();
    while (n != m) {
        n = n->parent();
        m = m->parent();
    }
    return n;
}

void Nest::deps(const Node* curr) const {
    if (curr->mut()) {
        for (auto local_mut : curr->mut()->mut_local_muts()) {
            if (auto local_node = mut2node(local_mut)) {
                if (local_node == curr)
                    local_node->link(local_node);
                else if (auto parent = local_node->parent()) {
                    if (auto curr_child = parent->curr_child) {
                        assert(parent->children().contains(curr_child->mut()));
                        curr_child->link(local_node);
                    }
                }
            }
        }
    }

    for (auto [_, child] : curr->children()) {
        curr->curr_child = child;
        deps(child);
        curr->curr_child = nullptr;
    }
}

void Nest::find_SCCs(const Node* curr) const {
    curr->find_SCCs();
    for (auto [_, child] : curr->children()) {
        child->loop_depth_ = child->is_recursive() ? curr->loop_depth() + 1 : curr->loop_depth();
        find_SCCs(child);
    }
}

void Nest::Node::find_SCCs() const {
    Stack stack;
    for (int i = 0; const auto& [_, node] : children())
        if (node->idx_ == Unvisited) i = node->tarjan(i, this, stack);
}

uint32_t Nest::Node::tarjan(uint32_t i, const Node* parent, Stack& stack) const {
    this->idx_ = this->low_ = i++;
    this->on_stack_         = true;
    stack.emplace(this);

    for (auto dep : this->depends()) {
        if (dep->idx_ == Unvisited) i = dep->tarjan(i, parent, stack);
        if (dep->on_stack_) this->low_ = std::min(this->low_, dep->low_);
    }

    if (this->idx_ == this->low_) {
        parent_->topo_.emplace_front(std::make_unique<SCC>());
        SCC* scc = parent_->topo_.front().get();
        const Node* node;
        int num = 0;
        do {
            node             = pop(stack);
            node->on_stack_  = false;
            node->recursive_ = true;
            node->low_       = this->idx_;
            ++num;

            scc->emplace(node);
            auto [_, ins] = parent_->SCCs_.emplace(node, scc);
            assert_unused(ins);
        } while (node != this);

        if (num == 1 && !this->depends().contains(this)) this->recursive_ = false;
    }

    return i;
}

} // namespace mim
