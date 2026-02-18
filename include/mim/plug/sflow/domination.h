#pragma once

#include "mim/plug/sflow/cfg.h"

namespace mim::plug::sflow {

/// Dominator tree for a CFG.
///
/// Calculated using Cooper-Harvey-Kennedy algorithm
/// from Cooper et al, "A Simple, Fast Dominance Algorithm".
/// https://www.clear.rice.edu/comp512/Lectures/Papers/TR06-33870-Dom.pdf
class DominatorTree {
public:
    using Node = CFG::Node;

    DominatorTree(CFG& cfg);

    Node* idom(Node* node) { return idoms_.contains(node) ? idoms_[node] : nullptr; }

private:
    void assign_names(Node* node);
    Node* intersect(Node* left, Node* right);

    int count_ = 0;
    std::vector<Node*> nodes_;
    absl::flat_hash_map<Node*, Node*> idoms_;
    absl::flat_hash_map<Node*, int> names_;
};

} // namespace mim::plug::sflow
