#pragma once

#include <memory>

#include "mim/analyses/cfg.h"

namespace mim {

class DomTree;

class Scheduler {
public:
    /// @name Construction
    ///@{
    Scheduler() = default;
    explicit Scheduler(const Nest&);
    Scheduler(const Scheduler&) = delete;
    Scheduler(Scheduler&& other) noexcept
        : Scheduler() {
        swap(*this, other);
    }
    Scheduler& operator=(Scheduler other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name Getters
    ///@{
    World& world() { return nest().world(); }
    const Nest& nest() const { return *nest_; }
    Def* root() const { return nest_->root()->mut(); }
    const CFG& cfg() const { return *cfg_; }
    const CFNode* cfg(Def* mut) const { return cfg()[mut]; }
    const DomTree& domtree() const { return *domtree_; }
    const Uses& uses(const Def* def) const {
        if (auto i = def2uses_.find(def); i != def2uses_.end()) return i->second;
        return empty_;
    }
    ///@}

    /// @name Compute Schedules
    ///@{
    Def* early(const Def*);
    Def* late(Def* curr, const Def*);
    Def* smart(Def* curr, const Def*);
    ///@}

    /// @name Schedule Mutabales
    /// Order of Mutables within a Scope.
    ///@{
    using Schedule = std::vector<Def*>;
    static Schedule schedule(const CFG&);
    ///@}

    friend void swap(Scheduler& s1, Scheduler& s2) noexcept {
        using std::swap;
        swap(s1.nest_, s2.nest_);
        swap(s1.cfg_, s2.cfg_);
        swap(s1.domtree_, s2.domtree_);
        swap(s1.early_, s2.early_);
        swap(s1.late_, s2.late_);
        swap(s1.smart_, s2.smart_);
        swap(s1.def2uses_, s2.def2uses_);
    }

private:
    const Nest* nest_ = nullptr;
    std::unique_ptr<const CFG> cfg_;
    const DomTree* domtree_ = nullptr;
    Uses empty_;
    DefMap<Def*> early_;
    DefMap<Def*> late_;
    DefMap<Def*> smart_;
    DefMap<Uses> def2uses_;
};

} // namespace mim
