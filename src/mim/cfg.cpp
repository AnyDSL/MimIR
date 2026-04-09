#include "mim/cfg.h"

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

} // namespace mim
