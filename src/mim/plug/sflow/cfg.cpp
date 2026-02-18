
#include "mim/lam.h"

namespace mim::plug::sflow {

using namespace mim;

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

    CFG(Lam* entry) { entry_ = build(entry); }

    ~CFG() {
        Set<Node*> all;
        for (auto& [_, nodes] : lam2node)
            all.insert(nodes.begin(), nodes.end());
        for (auto node : all)
            delete node;
    }

    /// Transforms the CFG into its limit graph by repeatedly applying
    /// - T_1: Deletion of self-loop
    /// - T_2: Merging of nodes a and b, where a is the only predecessor of b
    ///
    /// If the CFG is reducible, this results in a single node.
    ///
    /// Reference: Hecht, Ullman; "Flow Graph Reducibility"
    void reduce() {}

    /// Removes self-loop from given node, if present.
    void t1(Node* node) { node->succs.erase(node); }

    /// Merges given node with its predecessor, if there is only
    /// a single one.
    void t2(Node* node) {
        // The entry basically has another invisible predecessor
        // and should therefore not be merged ever.
        if (node == entry_) return;

        if (node->preds.size() != 1) return;

        Node* pred = *node->preds.begin();

        // Merge lams and update lam2node
        for (auto lam : node->lams) {
            pred->lams.insert(lam);
            lam2node[lam].erase(node);
            lam2node[lam].insert(pred);
        }

        // Move successors from node to pred
        pred->succs.erase(node);
        for (auto succ : node->succs) {
            pred->succs.insert(succ);
            succ->preds.erase(node);
            succ->preds.insert(pred);
        }

        delete node;
    }

    /// Duplicate given node(s) into a separate copy per predecessor.
    //
    /// It is allowed to pass in Node%s from a different CFG object, for example
    /// one representing the limit graph. In that case, all nodes representing
    /// Def%s from the given node are split. An error is thrown if any Def%s are
    /// not found or the worlds do not match.
    ///
    /// Can be used to make control flow reducible without performance impact, but
    /// may increase code size exponentially in regards to loop depth.
    void split(Node* node) {
        if (node->preds.size() <= 1) return;

        // Keep original node for first predecessor, create copies for the rest
        auto pred_it = node->preds.begin();
        ++pred_it; // skip first predecessor

        for (; pred_it != node->preds.end(); ++pred_it) {
            Node* pred = *pred_it;

            // Create copy with same lams
            Node* copy = new Node();
            for (auto lam : node->lams) {
                copy->lams.insert(lam);
                lam2node[lam].insert(copy);
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

private:
    Node* build(Lam* lam) {
        // only one node per lam
        if (lam2node.contains(lam)) return *lam2node[lam].begin();

        Node* node    = new Node(lam);
        lam2node[lam] = Set<Node*>{node};

        for (auto op : lam->deps())
            for (auto local_mut : op->local_muts())
                if (auto local_lam = local_mut->isa<Lam>()) {
                    Node* succ = build(local_lam);
                    node->succs.insert(succ);
                    succ->preds.insert(node);
                }

        return node;
    }

    Node* entry_;
    DefMap<Set<Node*>> lam2node;
};

} // namespace mim::plug::sflow
