#pragma once

#include <memory>

#include "mim/def.h"

#include "absl/container/flat_hash_map.h"

namespace mim {

/// A @p Scope represents a region of @p Def%s that are live from the view of an @p entry's @p Var.
/// Transitively, all user's of the @p entry's @p Var are pooled into this @p Scope (see @p defs()).
/// Both @p entry() is @em NOT part of the @p Scope itself.
class Scope {
public:
    Scope(const Scope&)     = delete;
    Scope& operator=(Scope) = delete;

    explicit Scope(Def* entry);
    ~Scope();

    /// @name getters
    ///@{
    World& world() const { return world_; }
    Def* entry() const { return entry_; }
    Sym sym() const { return entry_->sym(); }
    ///@}

    /// @name Def%s bound/free in this Scope
    ///@{
    bool bound(const Def* def) const { return bound().contains(def); }
    // clang-format off
    const DefSet& bound()     const { calc_bound(); return bound_;     } ///< All @p Def%s within this @p Scope.
    const DefSet& free_defs() const { calc_bound(); return free_defs_; } ///< All @em non-const @p Def%s @em directly referenced but @em not @p bound within this @p Scope. May also include @p Var%s or @em muts.
    const VarSet& free_vars() const { calc_free (); return free_vars_; } ///< All @p Var%s that occurr free in this @p Scope. Does @em not transitively contain any free @p Var%s from @p muts.
    const MutSet& free_muts() const { calc_free (); return free_muts_; } ///< All @em muts that occurr free in this @p Scope.
    // clang-format on
    ///@}

    /// Does @p mut's Var occurr free in @p def?
    static bool is_free(Def* mut, const Def* def);

private:
    void run();
    void calc_bound() const;
    void calc_free() const;

    World& world_;
    Def* entry_             = nullptr;
    mutable bool has_bound_ = false;
    mutable bool has_free_  = false;
    mutable DefSet bound_;
    mutable DefSet free_defs_;
    mutable VarSet free_vars_;
    mutable MutSet free_muts_;
};

} // namespace mim
