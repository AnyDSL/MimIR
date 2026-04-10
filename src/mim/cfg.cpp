#include "mim/cfg.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace mim {

void CFG::Node::follow_escaping(const App* app) {
    for (auto mut : app->arg()->local_muts()) {
        if (auto lam = mut->isa<Lam>()) {
            if (auto target = cfg_.visit(lam)) {
                succs_.insert(target);
                target->preds_.insert(this);
            }
        }
    }
}

bool CFG::Node::add_edge(const Lam* succ) {
    if (auto node = cfg_.visit(succ)) {
        succs_.insert(node);
        node->preds_.insert(this);
        return true;
    }
    return false;
}

void CFG::Node::init() {
    auto app = mut_->body()->as<App>();
    if (auto callee = app->callee()->isa<Lam>()) {
        if (!add_edge(callee)) follow_escaping(app);
    } else if (auto dispatch = Dispatch(app)) {
        bool follow = false;
        for (auto branch : dispatch.tuple()->ops())
            if (!add_edge(branch->as<Lam>())) follow = true;
        if (follow) follow_escaping(app);
    } else {
        follow_escaping(app);
    }
}

CFG::CFG(Lam* entry, bool include_closed)
    : world_(entry->world())
    , entry_(mut2node_.emplace(entry, std::unique_ptr<Node>(new Node(*this, entry))).first->second.get())
    , include_closed_(include_closed) {
    entry_->init();
    calc_dominance();
}

CFG::Node* CFG::visit(const Lam* mut) {
    if (auto var = entry_->mut_->has_var()) {
        if (mut->free_vars().contains(var) || (include_closed_ && mut->free_vars().empty())) {
            if (auto node = (*this)[mut]) return node;
            mut2node_.emplace(mut, std::unique_ptr<Node>(new Node(*this, mut)));
            mut2node_[mut]->init();
            return mut2node_[mut].get();
        }
    }
    return nullptr;
}

void CFG::assign_postorder_numbers() {
    size_t number = 0;
    absl::flat_hash_set<Node*> visited;

    std::function<void(Node*)> visit = [&](Node* node) {
        if (!visited.insert(node).second) return;
        for (auto succ : node->succs_)
            visit(succ);
        node->postorder_number_ = ++number;
    };

    visit(entry_);
}

/// Calculates dominance using Cooper-Harvey-Kennedy algorithm
/// from Cooper et al, "A Simple, Fast Dominance Algorithm".
/// https://www.clear.rice.edu/comp512/Lectures/Papers/TR06-33870-Dom.pdf
void CFG::calc_dominance() {
    assign_postorder_numbers();

    // entry dominates itself
    entry_->idom_ = entry_;

    // collect nodes in reverse postorder (skip entry)
    std::vector<Node*> nodes;
    nodes.reserve(mut2node_.size());
    for (auto& [_, node] : mut2node_)
        if (node.get() != entry_ && node->postorder_number_ != 0) nodes.push_back(node.get());
    std::ranges::sort(nodes, [](Node* a, Node* b) { return a->postorder_number_ > b->postorder_number_; });

    for (bool todo = true; todo;) {
        todo = false;
        for (auto node : nodes) {
            const Node* new_idom = nullptr;
            for (auto pred : node->preds_)
                if (pred->idom_) new_idom = new_idom ? lca(new_idom, pred) : pred;
            if (node->idom_ != new_idom) {
                node->idom_ = new_idom;
                todo        = true;
            }
        }
    }
}

const CFG::Node* CFG::lca(const Node* n, const Node* m) {
    while (n != m) {
        while (n->postorder_number_ < m->postorder_number_)
            n = n->idom_;
        while (m->postorder_number_ < n->postorder_number_)
            m = m->idom_;
    }
    return n;
}

} // namespace mim
