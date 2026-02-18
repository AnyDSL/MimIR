#pragma once

#include "mim/def.h"
#include "mim/lam.h"

namespace mim::plug::sflow {

/// Dominator tree for the basic blocks in a given %Lam.
///
/// Calculated using Cooper-Harvey-Kennedy algorithm
/// from Cooper et al, "A Simple, Fast Dominance Algorithm".
/// https://www.clear.rice.edu/comp512/Lectures/Papers/TR06-33870-Dom.pdf
class DominatorTree {
public:
    DominatorTree(Lam* root);

    const Def* idom(const Def* def) { return idoms_.contains(def) ? idoms_[def] : nullptr; }

private:
    void assign_names(Lam* lam);
    const Def* intersect(const Def* left, const Def* right);

    int count_ = 0;
    DefVec defs_;
    DefMap<DefVec> preds_;
    Def2Def idoms_;
    DefMap<int> names_;
};

} // namespace mim::plug::sflow
