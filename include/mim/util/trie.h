#pragma once

#include <fstream>

#include <fe/arena.h>

#include "mim/util/print.h"
#include "mim/util/util.h"
#include "mim/util/vector.h"

namespace mim {

template<class D> class Trie {
private:
    class Node {
    public:
        Node()
            : def_(nullptr)
            , parent_(nullptr)
            , size_(0)
            , min_(size_t(-1)) {
            aux_.min = 0;
            aux_.max = u32(-1);
        }

        Node(Node* parent, D* def)
            : def_(def)
            , parent_(parent)
            , size_(parent->size_ + 1)
            , min_(parent->def_ ? parent->min_ : def->tid()) {
            parent->link_to_child(this);
        }

        void dot(std::ostream& os) const {
            for (const auto& [def, child] : children_) {
                println(os, "n{} -> n{}", child->def_->tid(), def_ ? def_->tid() : u32(0xffff));
                child->dot(os);
            }
        }

    private:
        ///@name Getters
        ///@{
        constexpr bool empty() const noexcept { return def_ == nullptr; }
        constexpr bool is_root() const noexcept { return empty(); }
        ///@}

        ///@name parent
        ///@{
        // clang-format off
        constexpr const Node*  aux_parent() const noexcept { return aux_.parent && (aux_.parent->aux_.down == this || aux_.parent->aux_.up == this) ? aux_.parent : nullptr; }
        constexpr const Node* path_parent() const noexcept { return aux_.parent && (aux_.parent->aux_.down != this && aux_.parent->aux_.up != this) ? aux_.parent : nullptr; }
        // clang-format on
        ///@}

        ///@name Aux Tree (Splay Tree)
        ///@{

        /// [Splays](https://hackmd.io/@CharlieChuang/By-UlEPFS#Operation1) `this` to the root of its splay tree.
        constexpr void splay() const noexcept {
            while (auto p = aux_parent()) {
                if (auto pp = p->aux_parent()) {
                    if (p->aux_.down == this && pp->aux_.down == p) { // zig-zig
                        pp->ror();
                        p->ror();
                    } else if (p->aux_.up == this && pp->aux_.up == p) { // zag-zag
                        pp->rol();
                        p->rol();
                    } else if (p->aux_.down == this && pp->aux_.up == p) { // zig-zag
                        p->ror();
                        pp->rol();
                    } else { // zag-zig
                        assert(p->aux_.up == this && pp->aux_.down == p);
                        p->rol();
                        pp->ror();
                    }
                } else if (p->aux_.down == this) { // zig
                    p->ror();
                } else { // zag
                    assert(p->aux_.up == this);
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
        template<size_t l> constexpr void rotate() const noexcept {
            constexpr size_t r = (l + 1) % 2;
            constexpr auto child
                = [](const Node* n, size_t i) -> const Node*& { return i == 0 ? n->aux_.down : n->aux_.up; };

            auto x = this;
            auto p = x->aux_.parent;
            auto c = child(x, r);
            auto b = child(c, l);

            if (b) b->aux_.parent = x;

            if (p) {
                if (child(p, l) == x) {
                    child(p, l) = c;
                } else if (child(p, r) == x) {
                    child(p, r) = c;
                } else {
                    /* only path parent */;
                }
            }

            x->aux_.parent = c;
            c->aux_.parent = p;
            child(x, r)    = b;
            child(c, l)    = x;
        }

        constexpr void rol() const noexcept { return rotate<0>(); }
        constexpr void ror() const noexcept { return rotate<1>(); }

        constexpr void aggregate() const noexcept {
            if (!is_root()) {
                aux_.min = aux_.max = def_->tid();
                if (auto l = aux_.down) {
                    aux_.min = std::min(aux_.min, l->aux_.min);
                    aux_.max = std::max(aux_.max, l->aux_.max);
                }
                if (auto r = aux_.up) {
                    aux_.min = std::min(aux_.min, r->aux_.min);
                    aux_.max = std::max(aux_.max, r->aux_.max);
                }
            }
        }
        ///@}

        /// @name Link-Cut-Tree
        /// This is a simplified version of a Link-Cut-Tree without the Cut operation.
        ///@{

        /// Registers the edge `this -> child` in the *aux* tree.
        constexpr void link_to_child(Node* child) const noexcept {
            this->expose();
            child->expose();
            if (!child->aux_.up) {
                this->aux_.parent = child;
                child->aux_.up    = this;
            }
        }

        /// Make a preferred path from `this` to root while putting `this` at the root of the *aux* tree.
        /// @returns the last valid path_parent().
        constexpr const Node* expose() const noexcept {
            const Node* prev = nullptr;
            for (auto curr = this; curr; prev = curr, curr = curr->aux_.parent) {
                curr->splay();
                assert(!prev || prev->aux_.parent == curr);
                curr->aux_.down = prev;
                curr->aggregate();
            }
            splay();
            return prev;
        }

        /// Find root of `this` in *rep* tree.
        constexpr const Node* find_root() const noexcept {
            expose();
            auto curr = this;
            while (auto r = curr->aux_.up) curr = r;
            curr->splay();
            return curr;
        }

        /// Least Common Ancestor of `this` and @p other in the *rep* tree.
        /// @returns `nullptr`, if @p a and @p b are in different trees.
        constexpr const Node* lca(Node* other) const noexcept { return this->expose(), other->expose(); }

        /// Is `this` a descendant of `other` in the *rep* tree?
        /// Also `true`, if `this == other`.
        constexpr bool is_descendant_of(Node* other) const noexcept {
            if (this == other) return true;
            this->expose();
            other->splay();
            auto curr = this;
            while (auto p = curr->aux_parent()) curr = p;
            return curr == other;
        }
        ///@}

        D* def_;
        Node* parent_;
        size_t size_;
        size_t min_;
        GIDMap<D*, fe::Arena::Ptr<Node>> children_;

        struct {
            const Node* parent = nullptr; ///< parent or path-parent
            const Node* down   = nullptr; ///< left/deeper/down/leaf-direction
            const Node* up     = nullptr; ///< right/shallower/up/root-direction
            u32 min            = u32(-1);
            u32 max            = 0;
        } mutable aux_;

        friend class Trie;
    };

public:
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

            auto tid        = def->tid();
            const Node* cur = this->node_;
            cur->expose();
            if (tid < node_->aux_.min || tid > node_->aux_.max) return false; // Quick rejection

            while (cur) {
                if (cur->def_ == def) return true;
                cur = (!cur->def_ || tid > cur->def_->tid()) ? cur->aux_.down : cur->aux_.up;
            }

            return false;
        }

        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            if (!this->node_->lca(other.node_)->is_root()) return true;

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

        void dump() const {
            std::cout << '{';
            auto sep = "";
            for (auto i = *this; i; i = i.parent()) {
                std::cout << sep << (*i)->tid();
                sep = ", ";
            }
            std::cout << '}' << std::endl;
        }

    private:
        Node* node_ = nullptr;

        friend class Trie;
    };

    static_assert(std::forward_iterator<Set>);
    static_assert(std::ranges::range<Set>);

    Trie()
        : root_(make_node()) {}

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
    [[nodiscard]] constexpr Set insert(Set i, D* def) noexcept {
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
        if (a.node_->is_descendant_of(b.node_)) return a;
        if (b.node_->is_descendant_of(a.node_)) return b;

        auto aa = (*a)->tid() < (*b)->tid() ? a : a.parent();
        auto bb = (*a)->tid() > (*b)->tid() ? b : b.parent();
        return mount(merge(aa, bb), (*a)->tid() < (*b)->tid() ? *b : *a);
    }

    /// @name dot
    /// GraphViz output.
    ///@{
    void dot() const { dot("trie.dot"); }

    void dot(std::string s) const {
        auto os = std::ofstream(s);
        println(os, "digraph {{");
        println(os, "ordering=out;");
        println(os, "node [shape=box,style=filled];");
        root_->dot(os);
        println(os, "}}");
    }
    ///@}

    constexpr size_t max_depth() const noexcept { return max_depth(root_.get(), 0); }

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
    fe::Arena::Ptr<Node> make_node() { return arena_.mk<Node>(); }
    fe::Arena::Ptr<Node> make_node(Node* parent, D* def) { return arena_.mk<Node>(parent, def); }

    Node* mount(Set parent, D* def) {
        assert(def->tid() != 0);
        auto [i, ins] = parent.node_->children_.emplace(def, nullptr);
        if (ins) i->second = make_node(parent.node_, def);
        return i->second.get();
    }

    size_t max_depth(const Node* n, size_t depth) const noexcept {
        size_t res = depth;
        for (const auto& [_, child] : n->children_) res = std::max(res, max_depth(child.get(), depth + 1));
        return res;
    }

    fe::Arena arena_;
    u32 counter_ = 1;
    fe::Arena::Ptr<Node> root_;
};

} // namespace mim
