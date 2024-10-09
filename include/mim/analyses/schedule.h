#pragma once

#include "mim/analyses/cfg.h"

namespace mim {

template<bool> class DomTreeBase;
using DomTree = DomTreeBase<true>;

class Scheduler {
public:
    /// @name Construction
    ///@{
    Scheduler() = default;
    explicit Scheduler(const Scope&);
    Scheduler(const Scheduler&) = delete;
    Scheduler(Scheduler&& other) noexcept
        : Scheduler() {
        swap(*this, other);
    }
    Scheduler& operator=(Scheduler other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name Getters
    ///@{
    const Scope& scope() const { return *scope_; }
    const F_CFG& cfg() const { return *cfg_; }
    const CFNode* cfg(Def* mut) const { return cfg()[mut]; }
    const DomTree& domtree() const { return *domtree_; }
    const Uses& uses(const Def* def) const {
        auto i = def2uses_.find(def);
        assert(i != def2uses_.end());
        return i->second;
    }
    ///@}

    /// @name Compute Schedules
    ///@{
    Def* early(const Def*);
    Def* late(const Def*);
    Def* smart(const Def*);
    ///@}

    /// @name Schedule Mutabales
    ///@{
    /// Order of Mutables within a Scope.
    using Schedule = std::vector<Def*>;
    static Schedule schedule(const Scope&);
    ///@}

    friend void swap(Scheduler& s1, Scheduler& s2) noexcept {
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

} // namespace mim
