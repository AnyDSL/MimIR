#pragma once

#include <compare>

#include <fstream>

#include <fe/arena.h>

#include "mim/util/print.h"
#include "mim/util/util.h"
#include "mim/util/vector.h"

namespace mim {

template<class D> class Trie {
public:
    class Node {
    public:
        Node(Node* parent, D* def)
            : def_(def)
            , parent_(parent)
            , size_(parent ? parent->size_ + 1 : 0)
            , min_(parent ? (parent->def_ ? parent->min_ : def->tid()) : size_t(-1)) {}

        void dot(std::ostream& os) {
            for (const auto& [def, child] : children_) {
                println(os, "n{}_{} -> n{}_{}", def_ ? def_->tid() : u32(0xffff), this, child->def_->tid(),
                        child.get());
                child->dot(os);
            }
        }

    private:
        ///@name Splay Tree
        ///@{
        // clang-format off
        D* splay_p() { return aux_.p && (aux_.p->aux_.l == this || aux_.p->aux_.r == this) ? aux_.p : nullptr; }
        D* path_p()  { return aux_.p && (aux_.p->aux_.l != this && aux_.p->aux_.r != this) ? aux_.p : nullptr; }
        // clang-format on
        D* splay_l() { return aux_.l; }
        D* splay_r() { return aux_.r; }
        D*& child(size_t i) { return i == 0 ? aux_.l : aux_.r; }

        /// [Splays](https://hackmd.io/@CharlieChuang/By-UlEPFS#Operation1) `this` to the root of its splay tree.
        void splay() const {
            while (auto p = splay_p()) {
                if (auto pp = p->splay_parent()) {
                    if (p->aux_.l == this && pp->aux_.l == p) { // zig-zig
                        pp->ror();
                        p->ror();
                    } else if (p->aux_.r == this && pp->aux_.r == p) { // zag-zag
                        pp->rol();
                        p->rol();
                    } else if (p->aux_.l == this && pp->aux_.r == p) { // zig-zag
                        p->ror();
                        pp->rol();
                    } else { // zag-zig
                        assert(p->aux_.r == this && pp->aux_.l == p);
                        p->rol();
                        pp->ror();
                    }
                } else if (p->aux_.l == this) { // zig
                    p->ror();
                } else { // zag
                    assert(p->aux_.r == this);
                    p->rol();
                }
            }
        }

        /// Helpfer for Splay-Tree: rotate left/right:
        /// ```
        ///  | Left                  | Right                  |
        ///  |-----------------------|------------------------|
        ///  |   p              p    |       p          p     |
        ///  |   |              |    |       |          |     |
        ///  |   x              c    |       x          c     |
        ///  |  / \     ->     / \   |      / \   ->   / \    |
        ///  | a   c          x   d  |     c   a      d   x   |
        ///  |    / \        / \     |    / \            / \  |
        ///  |   b   d      a   b    |   d   b          b   a |
        ///  ```
        template<size_t l> void rot() const {
            constexpr size_t r = (l + 1) % 2;

            auto x = this;
            auto p = x->aux_.p;
            auto c = x->child(r);
            // auto b = c ? c->child(l) : nullptr;
            auto b = c->child(l);

            if (b) b->aux_.p = x;

            if (p) {
                if (p->child(l) == x) {
                    p->child(l) = c;
                } else if (p->child(r) == x) {
                    p->child(r) = c;
                } else {
                    /* only path parent */;
                }
            }

            x->aux_.p   = c;
            x->child(r) = b;

            // if (c) {
            c->aux_.p   = p;
            c->child(l) = x;
            //}

            // x->aggregate();
            // c->aggregate();
        }

        void rol() const { return rot<0>(); }
        void ror() const { return rot<1>(); }
        ///@}

        /// @name Link-Cut-Tree
        /// This is a simplified version of a Link-Cut-Tree without the Cut operation.
        ///@{

        /// Registers the edge `this -> child` in the *aux* tree.
        /// @warning It's the responsibility of the user to link it in the *rep* tree accordingly.
        void link(D* child) const {
            this->expose();
            child->expose();
            if (!child->aux_.r) {
                this->aux_.p  = child;
                child->aux_.r = this;
            }
            // this->aggregate();
        }

        /// Make a preferred path from `this` to root while putting `this` at the root of the *aux* tree.
        /// @returns the last valid LinkCutTree::path_parent.
        D* expose() const {
            D* prev = nullptr;
            for (auto curr = this; curr; prev = curr, curr = curr->aux_.p) {
                curr->splay();
                assert(!prev || prev->aux_.p == curr);
                curr->aux_.l = prev;
                curr->aggregate();
            }
            splay();
            return prev;
        }

        /// Find root of `this` in *rep* tree.
        D* find_root() const {
            expose();
            auto curr = this;
            while (auto r = curr->aux_.r) curr = r;
            curr->splay();
            return curr;
        }

        /// Least Common Ancestor of `this` and @p other in the *rep* tree.
        /// @returns `nullptr`, if @p a and @p b are in different trees.
        D* lca(D* other) const {
            // if (this == other) return other;
            // if (this->find_root() != other->find_root()) return nullptr;
            this->expose();
            return other->expose();
        }

        /// Is `this` a descendant of `other` in the *rep* tree?
        /// Also `true`, if `this == other`.
        bool is_descendant_of(D* other) const {
            if (this == other) return true;
            this->expose();
            other->splay();
            auto curr = this;
            while (auto p = curr->splay_parent()) curr = p;
            return curr == other;
        }
        ///@}

        D* def_;
        Node* parent_;
        size_t size_;
        size_t min_;
        GIDMap<D*, fe::Arena::Ptr<Node>> children_;

        struct {
            mutable const D* p = nullptr; ///< parent or path-parent
            mutable const D* l = nullptr; ///< left/deeper/down/leaf-direction
            mutable const D* r = nullptr; ///< right/shallower/up/root-direction
        } aux_;

        friend class Trie;
    };

    /// Points to a Set within the Trie.
    class Set {
    public:
        /// @name Iterator Properties
        ///@{
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = D*;
        using pointer           = D**;
        using reference         = D*&;
        ///@}

        /// @name Construction
        ///@{
        constexpr Set() noexcept           = default;
        constexpr Set(const Set&) noexcept = default;
        constexpr Set(Set&&) noexcept      = default;
        constexpr Set(Node* node) noexcept
            : node_(node) {}
        constexpr Set(const Trie& trie) noexcept
            : node_(trie.root_.get()) {}
        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr bool empty() const noexcept { return node_->def_ == nullptr; }
        constexpr bool is_root() const noexcept { return empty(); }
        constexpr Set parent() const noexcept { return node_->parent_; }
        constexpr size_t size() const noexcept { return node_ ? node_->size_ : 0; }
        constexpr size_t min() const noexcept { return node_->min_; }
        constexpr size_t max() const noexcept { return node_->def_->tid(); }
        constexpr size_t within(D* def) const noexcept { return min() <= def->tid() && def->tid() <= max(); }
        ///@}

        /// @name Check Membership
        ///@{
        bool contains(D* def) const noexcept {
            if (empty() || !within(def)) return false;
            for (auto i = *this; i; i = i.parent())
                if (i == def) return true;

            return false;
        }

        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            for (auto ai = *this, bi = other; ai && bi;) {
                if (*ai == *bi) return true;
                if (ai.min() > other.max() || ai.max() < bi.min()) return false;

                if ((*ai)->tid() > (*bi)->tid())
                    ai = ai.parent();
                else
                    bi = bi.parent();
            }

            return false;
        }
        ///@}

        void dump() const {
            std::cout << '{';
            auto sep = "";
            for (auto i = *this; i; i = i.parent()) {
                std::cout << sep << (*i)->tid();
                sep = ", ";
            }
            std::cout << '}' << std::endl;
        }

        /// @name Iterators
        /// There are two ways to iterate over a Set.
        /// Outside users will most likely need method 1 while the implementation often relies on method 2:
        /// 1. This will **exclude** the *empty root node* Trie::root():
        ///    ```
        ///    for (auto elem : set) do_sth(elem)
        ///    ```
        ///    All iterator-related functionality (begin(), end(), operator++(), operator++(int)) works with this
        ///    iteration scheme.
        /// 2. This will **include** the *empty root node* Trie::root():
        ///    ```
        ///    for (auto i = set; i; i = i.parent()) do_sth(*i)
        ///    ```
        ///@{
        Set begin() const { return is_root() ? Set() : *this; }
        Set end() const { return {}; }
        ///@}

        /// @name Increment
        /// @note These operations only change the *view* of this Set at the Trie; the Trie itself is **not** modified.
        ///@{
        constexpr Set& operator++() noexcept {
            node_ = node_->parent_;
            if (is_root()) node_ = nullptr;
            return *this;
        }

        constexpr Set operator++(int) noexcept {
            auto res = *this;
            this->operator++();
            return res;
        }
        ///@}

        /// @name Comparisons
        ///@{
        // clang-format off
        constexpr bool operator==(Set other) const noexcept { return this->node_ == other.node_; }
        constexpr bool operator!=(Set other) const noexcept { return this->node_ != other.node_; }
        constexpr bool operator==(const D* other) const noexcept { return !this->empty() && this->node_->def_ == other; }
        constexpr bool operator!=(const D* other) const noexcept { return  this->empty() || this->node_->def_ != other; }
        // clang-format on
        ///@}

        /// @name Conversions
        ///@{
        constexpr explicit operator bool() const noexcept { return !empty(); } ///< Is not empty?
        constexpr reference operator*() const noexcept { return node_->def_; }
        constexpr pointer operator->() const noexcept { return node_->def_; }
        ///@}

    private:
        Node* node_ = nullptr;

        friend class Trie;
    };

    static_assert(std::forward_iterator<Set>);
    static_assert(std::ranges::range<Set>);

    Trie()
        : root_(make_node(nullptr, 0)) {}

    constexpr const Set root() const noexcept { return root_.get(); }

    /// @name Set Operations
    /// @note All operations do **not** modify the input set(s); they create a **new** Set.
    ///@{
    [[nodiscard]] Set create() { return root_.get(); }

    /// Create a Set with a *single* @p def%ent: @f$\{def\}@f$.
    [[nodiscard]] Set create(D* def) { return mount(root_.get(), def->tid() == 0 ? set_tid(def) : def); }

    /// Create a Set with all elements in @p vec.
    template<class I> [[nodiscard]] Set create(Vector<D*> v) {
        // Sort in ascending tids but 0 goes last.
        std::ranges::sort(v, [](auto i, auto j) { return i->tid() != 0 && (j->tid() == 0 || i->tid() < j->tid()); });

        Set res = root();
        for (auto i = v.begin(), e = std::unique(v.begin(), v.end()); i != e; ++i) res = insert(*i);
        return res;
    }

    /// Yields @f$a \cup \{def\}@f$.
    constexpr Set insert(Set i, D* def) noexcept {
        if (def->tid() == 0) return mount(i.node_, set_tid(def));
        if (!i) return mount(root_.get(), def);
        if (*i == def) return i;
        if ((*i)->tid() < def->tid()) return mount(i.node_, def);
        return mount(insert(i.parent(), def), *i);
    }

    /// Yields @f$i \setminus def@f$.
    [[nodiscard]] Set erase(Set i, const D* def) {
        if (!i || def->tid() == 0 || !i.within(def)) return i;
        if (*i == def) return i.parent();
        return mount(erase(i.parent(), def), *i);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        auto aa = (*a)->tid() < (*b)->tid() ? a : a.parent();
        auto bb = (*a)->tid() > (*b)->tid() ? b : b.parent();
        return mount(merge(aa, bb), (*a)->tid() < (*b)->tid() ? *b : *a);
    }

    void dot() { dot("trie.dot"); }

    size_t max_depth() { return max_depth(root_.get(), 0); }

    size_t max_depth(const Node* n, size_t depth) {
        size_t res = depth;
        for (const auto& [_, child] : n->children_) res = std::max(res, max_depth(child.get(), depth + 1));
        return res;
    }

    void dot(std::string s) {
        auto os = std::ofstream(s);
        println(os, "digraph {{");
        println(os, "ordering=out;");
        println(os, "node [shape=box,style=filled];");
        root_->dot(os);
        println(os, "}}");
    }

    friend void swap(Trie& t1, Trie& t2) noexcept {
        using std::swap;
        // clang-format off
        swap(t1.arena_,   t2.arena_);
        swap(t1.root_,    t2.root_);
        swap(t1.counter_, t2.counter_);
        // clang-format on
    }

    D* set_tid(D* def) noexcept {
        assert(def->tid() == 0);
        def->tid_ = counter_++;
        return def;
    }

private:
    Node* mount(Set parent, D* def) {
        assert(def->tid() != 0);
        auto [i, ins] = parent.node_->children_.emplace(def, nullptr);
        if (ins) i->second = make_node(parent.node_, def);
        return i->second.get();
    }

    fe::Arena::Ptr<Node> make_node(Node* parent, D* def) { return arena_.mk<Node>(parent, def); }

    fe::Arena arena_;
    u32 counter_ = 1;
    fe::Arena::Ptr<Node> root_;
};

} // namespace mim
