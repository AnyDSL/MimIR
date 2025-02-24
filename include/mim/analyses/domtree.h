#pragma once

#include "mim/analyses/cfg.h"

namespace mim {

/// A Dominance Tree.
/// The template parameter @p forward determines
/// whether a regular dominance tree (@c true) or a post-dominance tree (@c false) should be constructed.
/// This template parameter is associated with @p CFG's @c forward parameter.
class DomTree {
public:
    DomTree(const DomTree&)     = delete;
    DomTree& operator=(DomTree) = delete;

    explicit DomTree(const CFG& cfg)
        : cfg_(cfg)
        , children_(cfg)
        , idoms_(cfg)
        , depth_(cfg) {
        create();
        depth(root(), 0);
    }

    const CFG& cfg() const { return cfg_; }
    size_t index(const CFNode* n) const { return cfg().index(n); }
    const auto& children(const CFNode* n) const { return children_[n]; }
    const CFNode* root() const { return *idoms_.begin(); }
    const CFNode* idom(const CFNode* n) const { return idoms_[n]; }
    int depth(const CFNode* n) const { return depth_[n]; }
    const CFNode* least_common_ancestor(const CFNode* i,
                                        const CFNode* j) const; ///< Returns the least common ancestor of @p i and @p j.

private:
    void create();
    void depth(const CFNode* n, int i);

    const CFG& cfg_;
    typename CFG::Map<Vector<const CFNode*>> children_;
    typename CFG::Map<const CFNode*> idoms_;
    typename CFG::Map<int> depth_;
};

} // namespace mim
