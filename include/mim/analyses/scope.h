#pragma once

#include <memory>

#include "mim/def.h"

#include "absl/container/flat_hash_map.h"

namespace mim {

class CFA;
class CFG;

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

    /// @name simple CFA to construct a CFG
    ///@{
    const CFG& cfg() const;
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
    mutable std::unique_ptr<const CFG> cfg_;
};

/// Builds a nesting tree of all *immutables*/binders.
class Nest {
public:
    class Node {
    public:
        Node(Def* mut, Node* parent)
            : mut_(mut)
            , parent_(parent) {
            if (parent) parent->children_.emplace(mut, this);
        }

        /// @name Getters
        ///@{
        bool is_root() const { return parent_ == nullptr; }
        Def* mut() const { return mut_; }
        Node* parent() { return parent_; }
        const Node* parent() const { return parent_; }
        ///@}

        /// @name Children
        ///@{
        const auto& children() const { return children_; }
        size_t num_children() const { return children_.size(); }
        const Node* child(Def* mut) const {
            if (auto i = children_.find(mut); i != children_.end()) return i->second;
            return nullptr;
        }
        ///@}

    private:
        void dot(Tab, std::ostream&) const;

        Def* mut_;
        Node* parent_;
        MutMap<const Node*> children_;
        Vars vars_;

        friend class Nest;
    };

    Nest(Def* root);

    /// @name Getters
    ///@{
    World& world() const { return world_; }
    const Node* root() const { return root_; }
    Vars vars() const; ///< All Var%s occuring in this Nest - computed lazily.
    ///@}

    /// @name Nodes
    ///@{
    const auto& nodes() const { return nodes_; }
    size_t num_nodes() const { return nodes_.size(); }
    const Node* node(Def* mut) const {
        if (auto i = nodes_.find(mut); i != nodes_.end()) return i->second.get();
        return nullptr;
    }
    const Node* operator[](Def* mut) const { return node(mut); }
    ///@}

    /// @name dot
    ///@{
    void dot(std::ostream& os) const;
    void dot(const char* file = nullptr) const;
    ///@}

private:
    Node* make_node(Def*, Node* parent = nullptr);
    Node* find_parent(Def*, Node*);

    World& world_;
    MutMap<std::unique_ptr<Node>> nodes_;
    Node* root_;
    mutable Vars vars_;
};

} // namespace mim
