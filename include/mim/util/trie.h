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
        D* def_;
        Node* parent_;
        size_t size_;
        size_t min_;
        GIDMap<D*, fe::Arena::Ptr<Node>> children_;

        friend class Trie;
    };

    /// Points to a Set within the Trie.
    /// There are two ways to iterate over a Set.
    /// Outside users will most likely need method 1 while the implementation often reilies on method 2:
    /// 1. This will **elide** the *empty root node* Trie::root():
    ///    ```
    ///    for (auto elem : set) do_sth(elem)
    ///    ```
    /// 2. This **will** visit the *empty root node* Trie::root():
    ///    ```
    ///    for (auto i = set; i; i = i.parent()) do_sth(*i)
    ///    ```
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
        ///@}

        /// @name Check Membership
        ///@{
        bool contains(const D* def) const noexcept {
            if (this->empty() || def->tid() < this->min() || def->tid() > (**this)->tid()) return false;
            for (auto i = *this; i; i = i.parent())
                if (i == def) return true;

            return false;
        }

        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            for (auto ai = *this, bi = other; ai && bi;) {
                if (*ai == *bi) return true;

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
        ///@{
        Set begin() const { return is_root() ? Set() : *this; }
        Set end() const { return {}; }
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

    /// Create a Set wih a *single* @p def%ent: @f$\{def\}@f$.
    [[nodiscard]] Set create(D* def) { return create(root_.get(), def); }

    /// Create a Set wih all elements in @p vec.
    template<class I> [[nodiscard]] Set create(Vector<D*> v) {
        // Sort in ascneding tids but 0 goes last.
        std::ranges::sort(v, [](auto i, auto j) { return i->tid() != 0 && (j->tid() == 0 || i->tid() < j->tid()); });

        Set res = root();
        for (auto i = v.begin(), e = std::unique(v.begin(), v.end()); i != e; ++i) res = insert(*i);
        return res;
    }

    /// Yields @f$a \cup \{def\}@f$.
    constexpr Set insert(Set i, D* def) noexcept {
        if (i.empty()) return create(root_.get(), def);
        if (*i == def) return i;
        if (def->tid() == 0) return create(i.node_, def);
        if ((*i)->tid() < def->tid()) return create_has_tid(i.node_, def);
        return create_has_tid(insert(i.parent(), def), *i);
    }

    /// Yields @f$i \setminus def@f$.
    [[nodiscard]] Set erase(Set i, const D* def) {
        if (def->tid() == 0 || i.is_root()) return i;
        if (*i == def) return i.parent();
        if ((*i)->tid() < def->tid()) return i;
        return create_has_tid(erase(i.parent(), def), *i);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        auto aa = (*a)->tid() < (*b)->tid() ? a : a.parent();
        auto bb = (*a)->tid() > (*b)->tid() ? b : b.parent();
        return create(merge(aa, bb), (*a)->tid() < (*b)->tid() ? *b : *a);
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

    static void set(const D* def, u32 tid) { def->tid_ = tid; }

private:
    Node* create(Set parent, D* def) {
        if (def->tid() == 0) set(def, counter_++);
        return create_has_tid(parent, def);
    }

    Node* create_has_tid(Set parent, D* def) {
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
