#pragma once

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
        const Node* parent() const { return parent_; }
        /// @warning May be `nullptr`, if it's a virtual root comprising several Node%s.
        Def* mut() const {
            assert(mut_ || is_root());
            return mut_;
        }
        uint32_t level() const { return level_; }
        uint32_t loop_depth() const { return loop_depth_; }
        std::string name() const { return mut() ? mut()->unique_name() : std::string("<virtual>"); }
        bool is_root() const { return parent_ == nullptr; }
        bool is_recursive() const { return recursive_; }
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
        void find_SCCs() const;
        uint32_t dfs(uint32_t, const Node*, Stack&) const;
        ///@}

        Def* mut_;
        const Node* parent_;
        size_t level_;
        mutable MutMap<const Node*> children_;
        mutable absl::flat_hash_set<const Node*> depends_;
        mutable absl::flat_hash_set<const Node*> controls_;
        mutable Vector<std::variant<const Node*, Vector<const Node*>>> topo_;

        /// @name implementaiton details
        ///@{
        mutable uint32_t idx_          = 0;
        mutable uint32_t low_          = 0;
        mutable uint32_t loop_depth_   = 0;
        mutable bool on_stack_         = false;
        mutable bool visited_          = false;
        mutable bool recursive_        = true;
        mutable const Node* curr_child = nullptr;
        ///@}

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
