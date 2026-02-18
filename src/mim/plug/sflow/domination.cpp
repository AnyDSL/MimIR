#include "mim/plug/sflow/domination.h"

#include <ranges>

namespace mim::plug::sflow {

using namespace std::views;

DominatorTree::DominatorTree(CFG& cfg) {
    assign_names(cfg.entry());

    // idoms[entry] = entry
    idoms_[cfg.entry()] = cfg.entry();

    nodes_.resize(count_);
    for (auto [node, index] : names_)
        nodes_[index] = node;

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto node : nodes_ | reverse | drop(1)) {
            Node* new_idom = nullptr;
            for (auto pred : node->preds)
                if (idoms_[pred]) new_idom = new_idom ? intersect(new_idom, pred) : pred;
            if (idoms_[node] != new_idom) {
                idoms_[node] = new_idom;
                changed      = true;
            }
        }
    }
}

void DominatorTree::assign_names(Node* node) {
    // Skip already visited
    if (idoms_.contains(node)) return;

    // Add placeholder idom
    idoms_[node] = nullptr;

    // Post-order traversal
    for (auto succ : node->succs)
        assign_names(succ);

    // Assign name
    names_[node] = count_++;
}

DominatorTree::Node* DominatorTree::intersect(Node* left, Node* right) {
    auto finger1 = left;
    auto finger2 = right;
    while (finger1 != finger2) {
        while (names_[finger1] < names_[finger2])
            finger1 = idoms_[finger1];
        while (names_[finger2] < names_[finger1])
            finger2 = idoms_[finger2];
    }
    return finger1;
}

} // namespace mim::plug::sflow
