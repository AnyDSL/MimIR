
#include <ranges>

#include "mim/def.h"
#include "mim/lam.h"
#include "mim/schedule.h"
namespace mim::plug::sflow {

using namespace mim;
using namespace std::views;

/// Dominator tree for the basic blocks in a given %Lam.
///
/// Calculated using Cooper-Harvey-Kennedy algorithm
/// from Cooper et al, "A Simple, Fast Dominance Algorithm".
/// https://www.clear.rice.edu/comp512/Lectures/Papers/TR06-33870-Dom.pdf
class DominatorTree {
public:
    DominatorTree(Lam* root) {
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

    const Def* idom(const Def* def) { return idoms_.contains(def) ? idoms_[def] : nullptr; }

private:
    // names all node with an index in post-order
    void assign_names(Lam* lam) {
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

    const Def* intersect(const Def* left, const Def* right) {
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

    int count_ = 0;
    DefVec defs_;
    DefMap<DefVec> preds_;
    Def2Def idoms_;

    DefMap<int> names_;
};

class Scheduler : mim::Scheduler {};

} // namespace mim::plug::sflow
