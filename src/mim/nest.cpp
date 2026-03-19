#include "mim/nest.h"

#include <ranges>

#include "mim/world.h"

namespace mim {
using std::ranges::views::reverse;

Nest::Nest(Def* r)
    : world_(r->world())
    , root_(make_node(r)) {
    populate();
}

Nest::Nest(View<Def*> muts)
    : world_(muts.front()->world())
    , root_(make_node(nullptr)) {
    for (auto mut : muts)
        make_node(mut, root_);
    populate();
}

Nest::Nest(World& world)
    : world_(world)
    , root_(make_node(nullptr)) {
    world.for_each(false, [this](Def* mut) { make_node(mut, root_); });
    populate();
}

void Nest::populate() {
    std::queue<Node*> queue;

    if (root()->mut())
        queue.push(root_);
    else
        for (auto child : root_->children().nodes())
            queue.push(child);

    while (!queue.empty()) {
        auto curr_node = pop(queue);
        for (auto op : curr_node->mut()->deps()) {
            for (auto local_mut : op->local_muts()) {
                if ((*this)[local_mut] || !contains(local_mut)) continue;

                if (curr_node->level() < local_mut->free_vars().size()) {
                    for (auto node = curr_node;; node = node->inest_) {
                        if (auto var = node->mut()->has_var()) {
                            if (local_mut->free_vars().contains(var)) {
                                queue.push(make_node(local_mut, node));
                                break;
                            }
                        }
                    }
                } else {
                    uint32_t max = 0;
                    auto inest   = root_;
                    for (auto var : local_mut->free_vars()) {
                        if (auto node = (*this)[var->mut()]; node && node->level() > max) {
                            max   = node->level();
                            inest = node;
                        }
                    }
                    queue.push(make_node(local_mut, inest));
                }
            }
        }
    }
}

Nest::Node* Nest::make_node(Def* mut, Node* inest) {
    auto node = std::unique_ptr<Node>(new Node(*this, mut, inest)); // can't use make_unique - c'tor is private
    auto res  = node.get();
    mut2node_.emplace(mut, std::move(node));
    if (mut) {
        if (auto var = mut->has_var()) vars_ = world().vars().insert(vars_, var);
    }
    return res;
}

const Nest::Node* Nest::lca(const Node* n, const Node* m) {
    while (n->level() < m->level())
        m = m->inest();
    while (m->level() < n->level())
        n = n->inest();
    while (n != m) {
        // Walk back sibling dependencies if there is only one user and it is
        // not part of a mutually recursive loop.
        // After [eta expansion](mim:EtaExpPhase), any function is either well-known
        // or only used once. Functions that are only used once do not have arguments,
        // as all of them get reduced to the expression passed by the single call site.
        // Walking up unique users is therefore correct, as any direct call from the
        // immediate nester must be unknown for siblings to be reachable as well, which
        // is not possible, meaning that control has to go through that single neighbor.
        // TODO: support the possibility to opt out from this
        if (!n->is_recursive() && n->sibl_deps<false>().num() == 1) {
            n = *n->sibl_deps<false>().begin();
            continue;
        }
        if (!m->is_recursive() && m->sibl_deps<false>().num() == 1) {
            m = *m->sibl_deps<false>().begin();
            continue;
        }
        n = n->inest();
        m = m->inest();
    }
    return n;
}

void Nest::calc_sibl_deps(Node* curr) const {
    if (curr->mut()) {
        for (auto op : curr->mut()->deps()) {
            for (auto local_mut : op->local_muts()) {
                if (auto local_node = const_cast<Nest&>(*this)[local_mut]) {
                    if (local_node == curr)
                        local_node->link(local_node);
                    else if (auto inest = local_node->inest()) {
                        if (auto curr_child = inest->curr_child) {
                            assert(inest->children().contains(curr_child->mut()));
                            curr_child->link(local_node);
                        }
                    }
                }
            }
        }
    }

    for (auto child : curr->children().nodes()) {
        curr->curr_child = child;
        calc_sibl_deps(child);
        curr->curr_child = nullptr;
    }
}

void Nest::calc_SCCs(Node* curr) const {
    curr->calc_SCCs();
    for (auto [_, child] : curr->children()) {
        child->loop_depth_ = child->is_recursive() ? curr->loop_depth() + 1 : curr->loop_depth();
        calc_SCCs(child);
    }
}

void Nest::Node::calc_SCCs() {
    Stack stack;
    for (int i = 0; auto& [_, node] : children())
        if (node->idx_ == Unvisited) i = node->tarjan(i, this, stack);
}

uint32_t Nest::Node::tarjan(uint32_t i, Node* inest, Stack& stack) {
    this->idx_ = this->low_ = i++;
    this->on_stack_         = true;
    stack.emplace(this);

    for (auto dep : this->sibl_deps_.nodes_) {
        if (dep->idx_ == Unvisited) i = dep->tarjan(i, inest, stack);
        if (dep->on_stack_) this->low_ = std::min(this->low_, dep->low_);
    }

    if (this->idx_ == this->low_) {
        inest_->topo_.emplace_front(std::make_unique<SCC>());
        SCC* scc = inest_->topo_.front().get();
        Node* node;
        int num = 0;
        do {
            node             = pop(stack);
            node->on_stack_  = false;
            node->recursive_ = true;
            node->low_       = this->idx_;
            ++num;

            scc->emplace(node);
            auto [_, ins] = inest_->SCCs_.emplace(node, scc);
            assert_unused(ins);
        } while (node != this);

        if (num == 1 && !this->sibl_deps().contains(this)) this->recursive_ = false;
    }

    return i;
}

/// Calculates dominance using Cooper-Harvey-Kennedy algorithm
/// from Cooper et al, "A Simple, Fast Dominance Algorithm".
/// https://www.clear.rice.edu/comp512/Lectures/Papers/TR06-33870-Dom.pdf
const Nest::Node& Nest::Node::with_dominance() const {
    if (idom_ || is_root() || !inest()->mut()) return *this;

    // Holds all siblings in reverse post-order coming from the parent
    std::vector<const Nest::Node*> nodes;

    // Initialize entry nodes directly referenced by the parent
    for (auto op : inest()->mut()->deps()) {
        for (auto mut : op->local_muts()) {
            if (auto node = nest()[mut]; node && node->inest() == inest()) {
                node->idom_             = inest();
                node->postorder_number_ = 0; // mark as visited
            }
        }
    }

    std::function<void(const Nest::Node*)> visit = [&](const Nest::Node* node) {
        if (node->postorder_number_ != size_t(-1)) return; // already visited
        node->postorder_number_ = 0;                       // mark in progress
        for (auto child : node->sibl_deps())
            visit(child);
        node->postorder_number_ = nodes.size();
        nodes.push_back(node);
    };

    // Assign post-order numbers via DFS from entry nodes
    for (auto op : inest()->mut()->deps()) {
        for (auto mut : op->local_muts()) {
            if (auto node = nest()[mut]; node && node->inest() == inest()) {
                node->postorder_number_ = size_t(-1); // reset so visit processes it
                visit(node);
            }
        }
    }

    // Temporarily give largest post-order number to parent (restored below)
    auto saved_parent_postorder = inest()->postorder_number_;
    inest()->postorder_number_  = nodes.size();

    auto intersect = [](const Nest::Node* finger1, const Nest::Node* finger2) {
        while (finger1 != finger2) {
            while (finger1->postorder_number_ < finger2->postorder_number_)
                finger1 = finger1->idom_;
            while (finger2->postorder_number_ < finger1->postorder_number_)
                finger2 = finger2->idom_;
        }
        return finger1;
    };

    // Actual dominance algorithm
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto node : nodes | reverse) {
            // skip entry nodes
            if (node->idom_ == inest()) continue;

            const Nest::Node* new_idom = nullptr;
            for (auto user : node->sibl_deps<false>())
                if (user->idom_) new_idom = new_idom ? intersect(new_idom, user) : user;
            if (node->idom_ != new_idom) {
                node->idom_ = new_idom;
                changed     = true;
            }
        }
    }

    inest()->postorder_number_ = saved_parent_postorder;
    return *this;
}

} // namespace mim
