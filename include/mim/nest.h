#pragma once

#include <memory>
#include <ranges>

#include "mim/def.h"

#include "mim/util/link_cut_tree.h"

namespace mim {

/// Builds a nesting tree of all *immutables*/binders.
class Nest {
public:
    class Node : private lct::Node<Node, int> {
    public:
        /// @name Getters
        ///@{
        std::string name() const { return mut() ? mut()->unique_name() : std::string("<virtual>"); }
        const Nest& nest() const { return nest_; }
        const Node* parent() const { return parent_; }
        bool is_root() const { return parent_ == nullptr; }
        /// The mutable capsulated in this Node or `nullptr`, if it's a *virtual root* comprising several Node%s.
        Def* mut() const {
            assert(mut_ || is_root());
            return mut_;
        }
        uint32_t level() const { return level_; }
        uint32_t loop_depth() const { return sccs().loop_depth_; }
        ///@}

        /// @name Children
        ///@{
        auto child_muts() const { return child_mut2node_ | std::views::keys; }
        auto child_nodes() const {
            return child_mut2node_
                 | std::views::transform([](const auto& pair) { return const_cast<const Node*>(pair.second); });
        }
        auto child_mut2node() const {
            return child_mut2node_ | std::views::transform([](const auto& pair) {
                       return std::pair{pair.first, const_cast<const Node*>(pair.second)};
                   });
        }
        const Node* child(Def* mut) const {
            if (auto i = child_mut2node_.find(mut); i != child_mut2node_.end()) return i->second;
            return nullptr;
        }
        size_t num_children() const { return child_mut2node_.size(); }
        ///@}

        /// @name depends/controls
        /// These are the dependencies across children():
        /// * A child `n` depends() on `m`, if a subtree of `n` uses `m`.
        /// * Conversly: `m` controls() `n`.
        ///@{
        auto depends() const {
            return deps().depends_ | std::views::transform([](Node* n) { return const_cast<const Node*>(n); });
        }
        auto controls() const {
            return deps().controls_ | std::views::transform([](Node* n) { return const_cast<const Node*>(n); });
        }
        size_t num_depends() const { return depends().size(); }
        ///@}

        /// Strongly Connected Component.
        using SCC = absl::flat_hash_set<const Node*>;
        /// @name SCCs
        /// [SCCs](https://en.wikipedia.org/wiki/Strongly_connected_component) for all children dependencies.
        /// @note The Nest::root() cannot be is_mutually_recursive() by definition.
        /// If you have a set of mutually recursive Def%s as "root", include them all by using a *virtual root*.
        ///@{
        const auto& SCCs() { return sccs().SCCs_; }
        const auto& topo() const { return sccs().topo_; } ///< Topological sorting of all SCCs.
        bool is_recursive() const { return sccs().recursive_; }
        bool is_mutually_recursive() const { return is_recursive() && parent_ && parent_->SCCs_[this]->size() > 1; }
        bool is_directly_recursive() const { return is_recursive() && (!parent_ || parent_->SCCs_[this]->size() == 1); }
        ///@}

    private:
        Node(const Nest& nest, Def* mut, Node* parent)
            : nest_(nest)
            , mut_(mut)
            , parent_(parent)
            , level_(parent ? parent->level() + 1 : 0) {
            if (parent) parent->child_mut2node_.emplace(mut, this);
        }

        const Node& deps() const { return nest().deps(), *this; }
        const Node& sccs() const { return nest().sccs(), *this; }

        void link(Node* node) { this->depends_.emplace(node), node->controls_.emplace(this); }
        void dot(Tab, std::ostream&) const;

        /// SCCs
        using Stack = std::stack<Node*>;
        void find_SCCs();
        uint32_t tarjan(uint32_t, Node*, Stack&);

        const Nest& nest_;
        Def* mut_;
        Node* parent_;
        uint32_t level_;
        uint32_t loop_depth_ : 31 = 0;
        bool recursive_      : 1  = false;
        MutMap<Node*> child_mut2node_;
        absl::flat_hash_set<Node*> depends_;
        absl::flat_hash_set<Node*> controls_;
        std::deque<std::unique_ptr<SCC>> topo_;
        absl::node_hash_map<const Node*, const SCC*> SCCs_;

        // implementaiton details
        static constexpr uint32_t Unvisited = uint32_t(-1);
        uint32_t idx_                       = Unvisited;
        uint32_t low_  : 31                 = 0;
        bool on_stack_ : 1                  = false;
        Node* curr_child                    = nullptr;

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
    const Node* root() const { return root_; }
    Vars vars() const { return vars_; } ///< All Var%s occurring in this Nest.
    bool contains(const Def* def) const { return has_intersection(vars(), def->free_vars()); }
    bool is_recursive() const { return sccs().root()->is_recursive(); }
    ///@}

    /// @name Nodes
    ///@{
    auto muts() const { return mut2node_ | std::views::keys; }
    auto nodes() const {
        return mut2node_ | std::views::transform([](const auto& pair) { return (const Node*)pair.second.get(); });
    }
    const auto& mut2node() const { return mut2node_; }
    size_t num_nodes() const { return mut2node_.size(); }
    const Node* mut2node(Def* mut) const { return mut2node_nonconst(mut); }
    const Node* operator[](Def* mut) const { return mut2node(mut); } ///< Same as above.
    ///@}

    static const Node* lca(const Node* n, const Node* m); ///< Least common ancestor of @p n and @p m.

    /// @name dot
    /// GraphViz output.
    ///@{
    void dot(std::ostream& os) const;
    void dot(const char* file = nullptr) const;
    void dot(std::string s) const { dot(s.c_str()); }
    ///@}

private:
    Node* mut2node_nonconst(Def* mut) const {
        if (auto i = mut2node_.find(mut); i != mut2node_.end()) return i->second.get();
        return nullptr;
    }

    void populate();
    Node* make_node(Def*, Node* parent = nullptr);
    void deps(Node*) const;
    void find_SCCs(Node*) const;

    const Nest& deps() const {
        if (!deps_) {
            deps_ = true;
            deps(root_);
        }
        return *this;
    }

    const Nest& sccs() const {
        if (!sccs_) {
            sccs_ = true;
            find_SCCs(root_);
        }
        return *this;
    }

    World& world_;
    absl::flat_hash_map<Def*, std::unique_ptr<Node>> mut2node_;
    Vars vars_;
    Node* root_;
    mutable bool deps_ = false;
    mutable bool sccs_ = false;
};

} // namespace mim
