#pragma once

#include <memory>

#include "mim/analyses/cfg.h"

namespace mim {

class DomTree;

/// References a user.
/// A Def `u` which uses Def `d` as `i^th` operand is a Use with Use::index `i` of Def `d`.
class Use {
public:
    static constexpr size_t Type = -1_s;

    Use() {}
    Use(const Def* def, size_t index)
        : def_(def)
        , index_(index) {}

    size_t index() const { return index_; }
    const Def* def() const { return def_; }
    operator const Def*() const { return def_; }
    const Def* operator->() const { return def_; }
    bool operator==(Use other) const { return this->def_ == other.def_ && this->index_ == other.index_; }

private:
    const Def* def_;
    size_t index_;
};

struct UseHash {
    inline size_t operator()(Use use) const {
        if constexpr (sizeof(size_t) == 8)
            return hash((u64(use.index())) << 32_u64 | u64(use->gid()));
        else
            return hash_combine(hash_begin(u16(use.index())), use->gid());
    }
};

struct UseEq {
    bool operator()(Use u1, Use u2) const { return u1 == u2; }
};

using Uses = absl::flat_hash_set<Use, UseHash, UseEq>;

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
    const Nest::Node* early(const Def*);
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
    DefMap<const Nest::Node*> early_;
    DefMap<Def*> late_;
    DefMap<Def*> smart_;
    DefMap<Uses> def2uses_;
};

} // namespace mim
