#pragma once

#include "mim/def.h"

#include "absl/container/flat_hash_set.h"

namespace mim {

/// Builds a nesting tree of all *immutables*/binders.
class Nest {
public:
    class Node {
    public:
        Node(Def* mut, const Node* parent)
            : mut_(mut)
            , parent_(parent)
            , depth_(parent ? parent->depth() + 1 : 0) {
            if (parent) parent->children_.emplace(mut, this);
        }

        /// @name Getters
        ///@{
        const Node* parent() const { return parent_; }
        /// @warning May be `nullptr`, if it's a virtual root comprising several Node%s.
        Def* mut() const {
            assert(mut_ || is_root());
            return mut_;
        }
        bool is_root() const { return parent_ == nullptr; }
        size_t depth() const { return depth_; }
        std::string name() const { return mut() ? mut()->unique_name() : std::string("<virtual>"); }
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

        /// @name depends/controls
        ///@{
        const auto& depends() const { return depends_; }
        const auto& controls() const { return controls_; }
        size_t num_depends() const { return depends().size(); }
        const auto& topo() const { return topo_; }
        ///@}

    private:
        void link(const Node* node) const {
            this->depends_.emplace(node);
            node->controls_.emplace(this);
        }
        void dot(Tab, std::ostream&) const;

        using Stack = std::stack<const Node*>;
        /// @name Find SCCs
        ///@{
        void tarjan() const;
        int dfs(int, const Node*, Stack&) const;
        ///@}

        struct {
            unsigned idx      : 30 = 0;
            unsigned on_stack : 1  = false;
            unsigned visited  : 1  = false;
            unsigned low      : 30 = 0;
            unsigned closed   : 1  = false;
            unsigned rec      : 1  = true;
            const Node* curr_child = nullptr;
        } mutable impl_;

        Def* mut_;
        const Node* parent_;
        size_t depth_;
        mutable MutMap<const Node*> children_;
        mutable absl::flat_hash_set<const Node*> depends_;
        mutable absl::flat_hash_set<const Node*> controls_;
        mutable Vector<std::variant<const Node*, Vector<const Node*>>> topo_;

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
    Node* root() { return root_; }
    const Node* root() const { return root_; }
    Vars vars() const { return vars_; } ///< All Var%s occurring in this Nest.
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
    void deps(const Node*);
    Node* make_node(Def*, const Node* parent = nullptr);
    Node* find_parent(Def*, const Node*);

    World& world_;
    absl::flat_hash_map<Def*, std::unique_ptr<const Node>> nodes_;
    Vars vars_;
    Node* root_;
};

} // namespace mim
