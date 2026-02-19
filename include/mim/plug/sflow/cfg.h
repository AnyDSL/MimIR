#pragma once

#include "mim/def.h"
#include "mim/lam.h"

namespace mim::plug::sflow {

template<typename K>
using Set = absl::flat_hash_set<K>;

/// Represents a control flow graph and provides functions
/// to manipulate it for the purpose of making it reducible.
class CFG {
public:
    struct Node {
        Node() = default;
        Node(Def* entry)
            : lams({entry}) {}

        DefSet lams;
        Set<Node*> preds;
        Set<Node*> succs;
    };

    CFG(Lam* entry);
    ~CFG();

    Node* entry() { return entry_; }

    /// Creates the limit graph of this CFG by copying it and calling
    /// [reduce](@ref CFG::reduce) on it.
    CFG limit() {
        CFG limit         = *this;
        limit.split_lams_ = false;
        limit.reduce();
        return limit;
    }

    /// Transforms the CFG into its limit graph by repeatedly applying
    /// - T_1: Deletion of self-loop
    /// - T_2: Merging of nodes a and b, where a is the only predecessor of b
    ///
    /// If the CFG is reducible, this results in a single node.
    ///
    /// Reference: Hecht, Ullman; "Flow Graph Reducibility"
    void reduce();

    /// Removes self-loop from given node, if present.
    bool t1(Node* node) { node->succs.erase(node); }

    /// Merges given node with its predecessor, if there is only
    /// a single one.
    bool t2(Node* node);

    /// Duplicate given node(s) into a separate copy per predecessor.
    //
    /// It is allowed to pass in Node%s from a different CFG object, for example
    /// one representing the limit graph. In that case, all nodes representing
    /// Def%s from the given node are split. An error is thrown if any Def%s are
    /// not found or the worlds do not match.
    ///
    /// Can be used to make control flow reducible without performance impact, but
    /// may increase code size exponentially in regards to loop depth.
    void split(Node* node);

private:
    Node* build(Lam* lam);

    bool reduce(Node* current, Set<Node*>& visited);

    Node* entry_;
    DefMap<Set<Node*>> lam2node;
};

} // namespace mim::plug::sflow
