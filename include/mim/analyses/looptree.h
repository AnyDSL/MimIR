#pragma once

#include <fe/cast.h>

#include "mim/analyses/cfg.h"

namespace mim {

class LoopTreeBuilder;

/// Calculates a loop nesting forest rooted at LoopTree::root_.
/// The implementation uses Steensgard's algorithm.
/// Check out G. Ramalingam, "On Loops, Dominators, and Dominance Frontiers", 1999, for more information.
class LoopTree {
public:
    class Head;

    /// Represents a node of a loop nesting forest.
    /// A LoopTree::Base consists of a set of header CFNode%s.
    /// The header CFNode%s are the set of CFNode%s not dominated by any other CFNode within the loop.
    /// The root node is a LoopTree::Head without any CFNode%s but further children and `depth_ -1`.
    /// Thus, the forest is pooled into a tree.
    class Base : public fe::RuntimeCast<Base> {
    public:
        Base(Head* parent, int depth, View<const CFNode*>);
        virtual ~Base() = default;

        int depth() const { return depth_; }
        const Head* parent() const { return parent_; }
        auto cf_nodes() const { return cf_nodes_.view(); }
        size_t num_cf_nodes() const { return cf_nodes().size(); }

    protected:
        Head* parent_;
        Vector<const CFNode*> cf_nodes_;
        int depth_;
    };

    /// A Head owns further nodes as children.
    class Head : public Base {
    private:
        Head(Head* parent, int depth, View<const CFNode*> cf_nodes)
            : Base(parent, depth, cf_nodes) {}

    public:
        auto children() const { return children_.view(); }
        const Base* child(size_t i) const { return children_[i].get(); }
        size_t num_children() const { return children().size(); }
        bool is_root() const { return Base::parent_ == 0; }

    private:
        Vector<std::unique_ptr<Base>> children_;

        friend class Base;
        friend class LoopTreeBuilder;
    };

    /// A Leaf only holds a single CFNode and does not have any children.
    class Leaf : public Base {
    private:
        Leaf(size_t index, Head* parent, int depth, View<const CFNode*> cf_nodes)
            : Base(parent, depth, cf_nodes)
            , index_(index) {
            assert(Leaf::num_cf_nodes() == 1);
        }

    public:
        const CFNode* cf_node() const { return Leaf::cf_nodes().front(); }
        /// Index of a DFS of LoopTree::Leaf%s.
        size_t index() const { return index_; }

    private:
        size_t index_;

        friend class LoopTreeBuilder;
    };

    LoopTree(const LoopTree&)     = delete;
    LoopTree& operator=(LoopTree) = delete;

    explicit LoopTree(const CFG& cfg);
    const CFG& cfg() const { return cfg_; }
    const Head* root() const { return root_.get(); }
    const Leaf* operator[](const CFNode* n) const { return find(leaves_, n); }

private:
    static void get_nodes(Vector<const Base*>& nodes, const Base* node) {
        nodes.push_back(node);
        if (auto head = node->isa<Head>())
            for (const auto& child : head->children()) get_nodes(nodes, child.get());
    }

    const CFG& cfg_;
    CFG::Map<Leaf*> leaves_;
    std::unique_ptr<Head> root_;

    friend std::ostream& operator<<(std::ostream&, const LoopTree::Base*);
    friend class LoopTreeBuilder;
};

} // namespace mim
