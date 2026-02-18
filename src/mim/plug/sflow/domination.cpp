#include "mim/plug/sflow/domination.h"

#include <ranges>

namespace mim::plug::sflow {

using namespace std::views;

DominatorTree::DominatorTree(Lam* root) {
    assign_names(root);

    // idoms[n_0] = n_0
    idoms_[root] = root;

    defs_.resize(count_);
    for (auto [def, index] : names_)
        defs_[index] = def;

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto def : defs_ | reverse | drop(1)) {
            const Def* new_idom = nullptr;
            for (auto pred : preds_[def])
                if (idoms_[pred]) new_idom = new_idom ? intersect(new_idom, pred) : pred;
            if (idoms_[def] != new_idom) {
                idoms_[def] = new_idom;
                changed     = true;
            }
        }
    }
}

void DominatorTree::assign_names(Lam* lam) {
    // Do not enter bodies of returning lams
    // as they are not relevant for in-lam control
    // flow. Return continuations also appear in local_muts
    // and are handled separately.
    if (lam->ret_pi()) return;

    // Skip already visited
    if (idoms_.contains(lam)) return;

    // Add placeholder idom
    idoms_[lam] = nullptr;

    // Post-order traversal
    for (auto op : lam->deps())
        for (auto local_mut : op->local_muts())
            if (auto local_lam = local_mut->isa<Lam>()) {
                // add predecessor
                if (!preds_.contains(local_lam)) preds_[local_lam] = DefVec();
                preds_[local_lam].push_back(lam);

                assign_names(local_lam);
            }

    // Assign name
    names_[lam] = count_++;
}

const Def* DominatorTree::intersect(const Def* left, const Def* right) {
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
