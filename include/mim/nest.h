#pragma once

#include <memory>

#include "mim/def.h"

namespace mim {

/// Builds a nesting tree of all *immutables*/binders.
class Nest {
public:
    class Node {
    public:
        Node(Def* mut, const Node* parent)
            : mut_(mut)
            , parent_(parent)
            , level_(parent ? parent->level() + 1 : 0) {
            if (parent) parent->children_.emplace(mut, this);
        }

        /// @name Getters
        ///@{
        std::string name() const { return mut() ? mut()->unique_name() : std::string("<virtual>"); }
        const Node* parent() const { return parent_; }
        /// @warning May be `nullptr`, if it's a virtual root comprising several Node%s.
        Def* mut() const {
            assert(mut_ || is_root());
            return mut_;
        }
        uint32_t level() const { return level_; }
        uint32_t loop_depth() const { return loop_depth_; }
        bool is_root() const { return parent_ == nullptr; }
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
        ///@}

        using SCC = absl::flat_hash_set<const Node*>;
        /// @name SCCs
        /// [SCCs](https://en.wikipedia.org/wiki/Strongly_connected_component) for all sibling dependencies.
        /// @note The Nest::root() cannot be is_mutually_recursive() by definition.
        /// If you have a set of mutually recursive Def%s, make sure to include them all in this Nest by using a virtual
        /// root.
        ///@{
        const auto& SCCs() { return SCCs_; }
        const auto& topo() const { return topo_; } ///< Topological sorting of all SCCs.
        bool is_recursive() const { return recursive_; }
        bool is_mutually_recursive() const { return is_recursive() && parent() && parent()->SCCs_[this]->size() > 1; }
        bool is_directly_recursive() const {
            return is_recursive() && (!parent() || parent()->SCCs_[this]->size() == 1);
        }
        ///@}

    private:
        using Stack = std::stack<const Node*>;

        void link(const Node* node) const {
            this->depends_.emplace(node);
            node->controls_.emplace(this);
        }
        void dot(Tab, std::ostream&) const;

        /// @name Find SCCs
        ///@{
        void find_SCCs() const;
        uint32_t dfs(uint32_t, const Node*, Stack&) const;
        ///@}

        Def* mut_;
        const Node* parent_;
        size_t level_;
        mutable MutMap<const Node*> children_;
        mutable absl::flat_hash_set<const Node*> depends_;
        mutable absl::flat_hash_set<const Node*> controls_;
        mutable std::deque<std::unique_ptr<SCC>> topo_;
        mutable absl::node_hash_map<const Node*, const SCC*> SCCs_;

        /// @name implementaiton details
        ///@{
        mutable uint32_t idx_          = 0;
        mutable uint32_t low_          = 0;
        mutable uint32_t loop_depth_   = 0;
        mutable bool on_stack_         = false;
        mutable bool visited_          = false;
        mutable bool recursive_        = false;
        mutable const Node* curr_child = nullptr;
        ///@}

        friend class Nest;
    };

    /// @name Constructors
    ///@{
    Nest(Def* root);
    Nest(View<Def*> muts); ///< Constructs a *virtual root* with @p muts as children.
    Nest(World&);          ///< *Virtual root* with all World::externals as children.
    ///@}

    /// @name Getters
    ///@{
    World& world() const { return world_; }
    Node* root() { return root_; }
    const Node* root() const { return root_; }
    Vars vars() const { return vars_; } ///< All Var%s occurring in this Nest.
    bool contains(const Def* def) const { return vars().intersects(def->free_vars()); }
    bool is_recursive() const { return root()->is_recursive(); }
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
    Node* make_node(Def*, const Node* parent = nullptr);
    void deps(const Node*);
    void find_SCCs(const Node*) const;

    World& world_;
    absl::flat_hash_map<Def*, std::unique_ptr<const Node>> nodes_;
    Vars vars_;
    Node* root_;
    bool deps_ = false;
};

} // namespace mim
