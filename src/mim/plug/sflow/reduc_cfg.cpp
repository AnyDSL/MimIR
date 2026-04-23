#include "mim/plug/sflow/reduc_cfg.h"

#include "mim/lam.h"
#include "mim/rewrite.h"
#include "mim/world.h"

namespace mim::plug::sflow {

CFG::CFG(Lam* entry) { entry_ = build(entry); }

CFG::~CFG() {
    for (auto [node, limit] : node2limit_) {
        limit->nodes.erase(node);
        if (limit->nodes.empty()) delete limit;
        delete node;
    }
}

CFG::LimitNode* CFG::limit() {
    if (limit_) return limit_;

    // Create new graph of LimitNodes
    for (auto [_, node] : lam2node_)
        node2limit_[node] = new LimitNode(node);
    // Copy edges
    for (auto [node, limit] : node2limit_) {
        for (auto pred : node->preds)
            limit->preds.insert(node2limit_[pred]);
        for (auto pred : node->succs)
            limit->succs.insert(node2limit_[pred]);
    }

    limit_ = node2limit_[entry_];

    Set<LimitNode*> visited;
    bool changed = true;
    while (changed)
        changed = reduce(limit_, visited);

    return limit_;
}

bool CFG::reduce(LimitNode* current, Set<LimitNode*>& visited) {
    visited.insert(current);
    bool changed = false;
    for (auto succ : current->succs) {
        changed |= reduce(succ, visited);
        changed |= t1(succ);
        changed |= t2(succ);
    }
    changed |= t1(current);
    visited.erase(current);
    return changed;
}

bool CFG::t2(LimitNode* lnode) {
    // The entry basically has another invisible predecessor
    // and should therefore not be merged ever.
    if (lnode == limit_) return false;

    if (lnode->preds.size() != 1) return false;

    auto pred = *lnode->preds.begin();

    // Merge nodes and update node2limit_
    for (auto node : lnode->nodes) {
        pred->nodes.insert(node);
        node2limit_[node] = pred;
    }

    // Move successors from node to pred
    pred->succs.erase(lnode);
    for (auto succ : lnode->succs) {
        pred->succs.insert(succ);
        succ->preds.erase(lnode);
        succ->preds.insert(pred);
    }

    delete lnode;

    return true;
}

/// Rewriter that substitutes mapped Defs but leaves unmapped mutables untouched.
class SplitRewriter : public Rewriter {
public:
    // Constructor taking a DefSet
    SplitRewriter(World& world, DefSet todo)
        : Rewriter(world)
        , todo_(std::move(todo)) {}

    // Constructor taking any number of const Def*
    template<std::same_as<const Def*>... Defs>
    SplitRewriter(World& world, Defs... defs)
        : Rewriter(world)
        , todo_{defs...} {}

    const Def* rewrite_mut(Def* mut) override {
        if (auto new_mut = lookup(mut)) return new_mut;
        if (auto lam = mut->isa<Lam>())
            if (todo_.contains(mut)) return rewrite_mut_Lam(lam);
        return mut;
    }

private:
    DefSet todo_;
};

/// Copies a mut Lam
Lam* split_lam(Lam* lam) {
    auto& world = lam->world();
    auto copy   = world.mut_lam(lam->type()->as<Pi>());

    if (auto var = lam->has_var()) {
        VarRewriter rw(var, copy->var());
        copy->set_filter(rw.rewrite(lam->filter()));
        copy->set_body(rw.rewrite(lam->body()));
    } else {
        copy->set_filter(lam->filter());
        copy->set_body(lam->body());
    }

    return copy;
}

void CFG::split(LimitNode* lnode) {
    if (lnode->preds.size() <= 1) return;

    // Keep original node for first predecessor, create copies for the rest
    auto pred_it = lnode->preds.begin();
    ++pred_it; // skip first predecessor

    for (; pred_it != lnode->preds.end(); ++pred_it) {
        LimitNode* pred = *pred_it;

        // Create copy
        LimitNode* copy = new LimitNode();

        Map<Node*, Node*> node2copy;

        // Duplicate nodes
        for (auto node : lnode->nodes) {
            Lam* lam_copy       = split_lam(node->lam);
            Node* node_copy     = new Node(lam_copy);
            node2copy[node]     = node_copy;
            lam2node_[lam_copy] = node_copy;
            copy->nodes.insert(node_copy);
        }

        // Rewire internal edges within limit node
        for (auto node : lnode->nodes) {
            Node* copy = node2copy[node];
            for (auto pred : node->preds)
                if (lnode->nodes.contains(pred)) {
                    copy->preds.insert(node2copy[pred]);
                    rewire(pred->lam, node->lam, copy->lam);
                }
            for (auto succ : node->succs)
                if (lnode->nodes.contains(succ)) copy->succs.insert(node2copy[succ]);
        }

        // Update predecessor's bodies to reference lam_copy instead of lam
        for (auto pred : pred->nodes) {
            SplitRewriter rw(pred_lam->world());
            rw.map(lam, lam_copy);
            pred_lam->set_body(rw.rewrite(pred_lam->body()));
        }

        // Link copy to this predecessor only
        copy->preds.insert(pred);
        pred->succs.erase(lnode);
        pred->succs.insert(copy);

        // Copy successor edges
        for (auto succ : lnode->succs) {
            copy->succs.insert(succ);
            succ->preds.insert(copy);
        }
    }

    // Original node keeps only the first predecessor
    Node* first_pred = *lnode->preds.begin();
    lnode->preds.clear();
    lnode->preds.insert(first_pred);
}

CFG::Node* CFG::build(Lam* lam) {
    // only one node per lam
    if (lam2node_.contains(lam)) return lam2node_[lam];

    Node* node     = new Node(lam);
    lam2node_[lam] = node;

    for (auto op : lam->deps())
        for (auto local_mut : op->local_muts())
            if (auto local_lam = local_mut->isa<Lam>()) {
                Node* succ = build(local_lam);
                node->succs.insert(succ);
                succ->preds.insert(node);
            }

    return node;
}

} // namespace mim::plug::sflow
