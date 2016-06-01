#ifndef THORIN_ANALYSES_DOMFRONTIER_H
#define THORIN_ANALYSES_DOMFRONTIER_H

#include "thorin/analyses/cfg.h"

namespace thorin {

/**
 * A Dominance Frontier Graph.
 * The template parameter @p forward determines whether to compute
 * regular dominance frontiers or post-dominance frontiers (i.e. control dependence).
 * This template parameter is associated with @p CFG's @c forward parameter.
 */
template<bool forward>
class DomFrontierBase : public YComp {
public:
    DomFrontierBase(const DomFrontierBase &) = delete;
    DomFrontierBase& operator=(DomFrontierBase) = delete;

    explicit DomFrontierBase(const CFG<forward> &cfg)
        : YComp(cfg.scope(), forward ? "dom_frontier" : "control_dependencies")
        , cfg_(cfg)
        , preds_(cfg)
        , succs_(cfg)
    {
        create();
    }
    static const DomFrontierBase& create(const Scope& scope) { return scope.cfg<forward>().domfrontier(); }

    const CFG<forward>& cfg() const { return cfg_; }
    const std::vector<const CFNode*>& preds(const CFNode* n) const { return preds_[n]; }
    const std::vector<const CFNode*>& succs(const CFNode* n) const { return succs_[n]; }
    virtual void stream_ycomp(std::ostream& out) const override;

private:
    void create();
    void link(const CFNode* src, const CFNode* dst) {
        succs_[src].push_back(dst);
        preds_[dst].push_back(src);
    }

    const CFG<forward>& cfg_;
    typename CFG<forward>::template Map<std::vector<const CFNode*>> preds_;
    typename CFG<forward>::template Map<std::vector<const CFNode*>> succs_;
};

typedef DomFrontierBase<true>  DomFrontiers;
typedef DomFrontierBase<false> ControlDeps;

}

#endif
