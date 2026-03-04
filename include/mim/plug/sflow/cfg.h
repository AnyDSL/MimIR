#pragma once

#include "mim/def.h"
#include "mim/lam.h"

#include "absl/container/flat_hash_map.h"

namespace mim::plug::sflow {

template<typename K>
using Set = absl::flat_hash_set<K>;
template<typename K, typename V>
using Map = absl::flat_hash_map<K, V>;

/// Represents a control flow graph and its limit graph.
///
/// Used by the Reduciblifier% to perform controlled node splitting.
/// Janssen and Corporaal, 1997, Making Graphs Reducible with Controlled Node Splitting
class CFG {
public:
    /// A node in a control flow graph.
    class Node {
    public:
        Node() = default;
        Node(Lam* entry)
            : lam(entry) {}

        Lam* lam;
        Set<Node*> preds;
        Set<Node*> succs;
    };

    /// A node in a CFG's limit graph.
    class LimitNode {
    public:
        LimitNode() = default;
        LimitNode(Node* node)
            : nodes({node}) {}

        bool t2(LimitNode* succ);

        void split();

        Set<Node*> nodes;
        Set<LimitNode*> preds;
        Set<LimitNode*> succs;
    };
    CFG(Lam* entry);
    ~CFG();

    Node* entry() { return entry_; }

    /// Calculates the CFG's limit graph by repeatedly applying
    /// - T_1: Deletion of self-loop
    /// - T_2: Merging of nodes a and b, where a is the only predecessor of b
    ///
    /// If the CFG is reducible, this results in a single node.
    ///
    /// Reference: Hecht, Ullman; "Flow Graph Reducibility"
    LimitNode* limit();

    /// Removes self-loop from given node, if present.
    bool t1(LimitNode* node) { return node->succs.erase(node); }

    /// Merges given node with its predecessor, if there is only
    /// a single one.
    bool t2(LimitNode* node);

    /// Duplicate given node(s) into a separate copy per predecessor.
    //
    /// It is allowed to pass in Node%s from a different CFG object, for example
    /// one representing the limit graph. In that case, all nodes representing
    /// Def%s from the given node are split. An error is thrown if any Def%s are
    /// not found or the worlds do not match.
    ///
    /// Can be used to make control flow reducible without performance impact, but
    /// may increase code size exponentially in regards to loop depth.
    void split(LimitNode* node);

private:
    Node* build(Lam* lam);

    bool reduce(LimitNode* current, Set<LimitNode*>& visited);

    Node* entry_;
    LimitNode* limit_;
    DefMap<Node*> lam2node_;
    Map<Node*, LimitNode*> node2limit_;
};

} // namespace mim::plug::sflow
