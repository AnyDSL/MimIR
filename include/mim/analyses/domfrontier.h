#pragma once

#include "mim/analyses/cfg.h"

namespace mim {

/// A Dominance Frontier Graph.
/// The template parameter @p forward determines whether to compute
/// regular dominance frontiers or post-dominance frontiers (i.e. control dependence).
/// This template parameter is associated with @p CFG's @c forward parameter.
/// See Cooper et al, 2001. A Simple, Fast Dominance Algorithm: http://www.cs.rice.edu/~keith/EMBED/dom.pdf
class DomFrontier {
public:
    DomFrontier(const DomFrontier&)     = delete;
    DomFrontier& operator=(DomFrontier) = delete;

    explicit DomFrontier(const CFG& cfg)
        : cfg_(cfg)
        , preds_(cfg)
        , succs_(cfg) {
        create();
    }

    const CFG& cfg() const { return cfg_; }
    const auto& preds(const CFNode* n) const { return preds_[n]; }
    const auto& succs(const CFNode* n) const { return succs_[n]; }

private:
    void create();
    void link(const CFNode* src, const CFNode* dst) {
        succs_[src].push_back(dst);
        preds_[dst].push_back(src);
    }

    const CFG& cfg_;
    typename CFG::Map<Vector<const CFNode*>> preds_;
    typename CFG::Map<Vector<const CFNode*>> succs_;
};

} // namespace mim
