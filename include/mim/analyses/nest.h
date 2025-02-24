#pragma once

#include "mim/def.h"

namespace mim {

/// Builds a nesting tree of all *immutables*/binders.
class Nest {
public:
    class Node {
    public:
        Node(Def* mut, Node* parent)
            : mut_(mut)
            , parent_(parent)
            , depth_(parent ? parent->depth() + 1 : 0) {
            if (parent) parent->children_.emplace(mut, this);
        }

        /// @name Getters
        ///@{
        Node* parent() { return parent_; }
        const Node* parent() const { return parent_; }
        /// @warning May be `nullptr`, if it's a virtual root comprising several Node%s.
        Def* mut() const {
            assert(mut_ || is_root());
            return mut_;
        }
        bool is_root() const { return parent_ == nullptr; }
        size_t depth() const { return depth_; }
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
        size_t depth_;
        MutMap<const Node*> children_;
        Vars vars_;

        friend class Nest;
    };

    /// @name Constructors
    ///@{
    Nest(Def* root);
    Nest(View<Def*> muts); ///< Constructs a virtual root with @p muts as children.
    Nest(World&);          ///< Virtual root with all World::externals as children.
    ///@}

    /// @name Getters
    ///@{
    World& world() const { return world_; }
    const Node* root() const { return root_; }
    Vars vars() const; ///< All Var%s occurring in this Nest - computed lazily.
    bool contains(const Def* def) const { return vars().intersects(def->free_vars()); }
    bool is_recursive() const;
    ///@}

    /// @name Nodes
    ///@{
    const auto& nodes() const { return nodes_; }
    size_t num_nodes() const { return nodes_.size(); }
    const Node* mut2node(Def* mut) const {
        if (auto i = nodes_.find(mut); i != nodes_.end()) return i->second.get();
        return nullptr;
    }
    const Node* operator[](Def* mut) const { return mut2node(mut); } ///< Same as above.
    ///@}

    static const Node* lca(const Node* n, const Node* m); ///< Least common ancestor of @p n and @p m.

    /// @name dot
    ///@{
    void dot(std::ostream& os) const;
    void dot(const char* file = nullptr) const;
    ///@}

private:
    void populate();
    Node* make_node(Def*, Node* parent = nullptr);
    Node* find_parent(Def*, Node*);

    World& world_;
    absl::flat_hash_map<Def*, std::unique_ptr<Node>> nodes_;
    Node* root_;
    mutable Vars vars_;
};

} // namespace mim
