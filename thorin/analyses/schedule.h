#pragma once

#include "thorin/analyses/cfg.h"

namespace thorin {

template<bool>
class DomTreeBase;
using DomTree = DomTreeBase<true>;

class Scheduler {
public:
    Scheduler() = default;
    explicit Scheduler(const Scope&);

    /// @name getters
    ///@{
    const Scope& scope() const { return *scope_; }
    const F_CFG& cfg() const { return *cfg_; }
    const CFNode* cfg(Def* nom) const { return cfg()[nom]; }
    const DomTree& domtree() const { return *domtree_; }
    const Uses& uses(const Def* def) const {
        auto i = def2uses_.find(def);
        assert(i != def2uses_.end());
        return i->second;
    }
    ///@}

    /// @name compute schedules
    ///@{
    Def* early(const Def*);
    Def* late(const Def*);
    Def* smart(const Def*);
    ///@}

    friend void swap(Scheduler& s1, Scheduler& s2) {
        using std::swap;
        swap(s1.scope_, s2.scope_);
        swap(s1.cfg_, s2.cfg_);
        swap(s1.domtree_, s2.domtree_);
        swap(s1.early_, s2.early_);
        swap(s1.late_, s2.late_);
        swap(s1.smart_, s2.smart_);
        swap(s1.def2uses_, s2.def2uses_);
    }

private:
    const Scope* scope_     = nullptr;
    const F_CFG* cfg_       = nullptr;
    const DomTree* domtree_ = nullptr;
    DefMap<Def*> early_;
    DefMap<Def*> late_;
    DefMap<Def*> smart_;
    DefMap<Uses> def2uses_;
};

using Schedule = std::vector<Def*>;
Schedule schedule(const Scope&);

} // namespace thorin
