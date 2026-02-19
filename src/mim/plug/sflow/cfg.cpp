#include "mim/plug/sflow/cfg.h"

#include "mim/lam.h"
#include "mim/rewrite.h"
#include "mim/world.h"

namespace mim::plug::sflow {

CFG::CFG(Lam* entry) { entry_ = build(entry); }

CFG::~CFG() {
    Set<Node*> all;
    for (auto& [_, nodes] : lam2node_)
        all.insert(nodes.begin(), nodes.end());
    for (auto node : all)
        delete node;
}

void CFG::reduce() {
    Set<Node*> visited;
    bool changed = true;
    while (changed)
        changed = reduce(entry(), visited);
}

bool CFG::reduce(Node* current, Set<Node*>& visited) {
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

bool CFG::t2(Node* node) {
    // The entry basically has another invisible predecessor
    // and should therefore not be merged ever.
    if (node == entry_) return false;

    if (node->preds.size() != 1) return false;

    Node* pred = *node->preds.begin();

    // Merge lams and update lam2node
    for (auto lam : node->lams) {
        pred->lams.insert(lam);
        lam2node_[lam].erase(node);
        lam2node_[lam].insert(pred);
    }

    // Move successors from node to pred
    pred->succs.erase(node);
    for (auto succ : node->succs) {
        pred->succs.insert(succ);
        succ->preds.erase(node);
        succ->preds.insert(pred);
    }

    delete node;

    return true;
}

/// Rewriter that substitutes mapped Defs but leaves unmapped mutables untouched.
class SubstRewriter : public Rewriter {
public:
    using Rewriter::Rewriter;

    const Def* rewrite_mut(Def* mut) override {
        if (auto new_def = lookup(mut)) return new_def;
        return mut;
    }
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

void CFG::split(Node* node) {
    if (node->preds.size() <= 1) return;

    // Keep original node for first predecessor, create copies for the rest
    auto pred_it = node->preds.begin();
    ++pred_it; // skip first predecessor

    for (; pred_it != node->preds.end(); ++pred_it) {
        Node* pred = *pred_it;

        // Create copy with same lams
        Node* copy = new Node();
        if (split_lams_) {
            // Actually duplicate Lams
            for (auto lam : node->lams) {
                Lam* lam_copy = split_lam(lam);
                copy->lams.insert(lam_copy);
                lam2node_[lam_copy].insert(copy);

                // Update predecessor's bodies to reference lam_copy instead of lam
                for (auto pred_lam : pred->lams) {
                    SubstRewriter rw(pred_lam->world());
                    rw.map(lam, lam_copy);
                    pred_lam->set_body(rw.rewrite(pred_lam->body()));
                }
            }
        } else {
            for (auto lam : node->lams) {
                copy->lams.insert(lam);
                lam2node_[lam].insert(copy);
            }
        }

        // Link copy to this predecessor only
        copy->preds.insert(pred);
        pred->succs.erase(node);
        pred->succs.insert(copy);

        // Copy successor edges
        for (auto succ : node->succs) {
            copy->succs.insert(succ);
            succ->preds.insert(copy);
        }
    }

    // Original node keeps only the first predecessor
    Node* first_pred = *node->preds.begin();
    node->preds.clear();
    node->preds.insert(first_pred);
}

CFG::Node* CFG::build(Lam* lam) {
    // only one node per lam
    if (lam2node_.contains(lam)) return *lam2node_[lam].begin();

    Node* node     = new Node(lam);
    lam2node_[lam] = Set<Node*>{node};

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
