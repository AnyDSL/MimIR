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
    bool contains(const Def* def) const { return vars().intersects(def->free_vars()); }
    bool is_recursive() const;
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
