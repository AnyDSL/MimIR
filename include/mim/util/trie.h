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
            for (auto [def, child] : children_) {
                println(os, "n{}_{} -> n{}_{}", def_ ? def_->tid() : u32(0xffff), this, child->def_->tid(), child);
                child->dot(os);
            }
        }

    private:
        D* def_;
        Node* parent_;
        size_t size_;
        size_t min_;
        GIDMap<D*, Node*> children_;

        friend class Trie;
    };

    /// Also serves as iterator.
    /// `set++` will remove the last defent from the set by moving the internal Node pointer one parent up.
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
        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr bool empty() const noexcept { return node_ == nullptr; }
        constexpr Set parent() const noexcept { return node_->parent_; }
        constexpr size_t size() const noexcept { return node_->size_; }
        constexpr size_t min() const noexcept { return node_->min_; }
        constexpr bool contains(const D* def) const noexcept {
            if (this->empty() || def->tid() < this->min() || def->tid() > (**this)->tid()) return false;
            for (auto i = *this; !i.empty(); ++i)
                if (i == def) return true;
            return false;
        }
        ///@}

        /// @name Setters
        ///@{
        constexpr void clear() noexcept { node_ = nullptr; }
        ///@}

        /// @name Iterators
        ///@{
        constexpr Set begin() const noexcept { return node_; }
        constexpr Set end() const noexcept { return {}; }
        constexpr Set cbegin() const noexcept { return begin(); }
        constexpr Set cend() const noexcept { return end(); }
        ///@}

        /// @name Comparisons
        ///@{
        // clang-format off
        constexpr bool operator==(Set other) const noexcept { return  (this->empty() && other.empty()) || this->node_ == other.node_; }
        constexpr bool operator!=(Set other) const noexcept { return !(this->empty() && other.empty()) && this->node_ != other.node_; }
        constexpr bool operator==(const D* other) const noexcept { return !this->empty() && this->node_->def_ == other; }
        constexpr bool operator!=(const D* other) const noexcept { return  this->empty() || this->node_->def_ != other; }
        // clang-format on
        constexpr auto operator<=>(Set other) const noexcept {
            if (this->empty() && other.empty()) return std::strong_ordering::equal;
            if (this->empty()) return std::strong_ordering::less;
            if (other.empty()) return std::strong_ordering::greater;
            return (**this)->tid() <=> (*other)->tid();
        }
        constexpr auto operator<=>(const D* other) const noexcept {
            if (this->empty()) return std::strong_ordering::less;
            return (**this)->tid() <=> other->tid();
        }
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
        constexpr Set operator++(int) noexcept {
            auto res = *this;
            node_    = node_->parent_;
            return res;
        }
        constexpr Set& operator++() noexcept { return node_ = node_->parent_, *this; }
        ///@}

    private:
        Node* node_ = nullptr;

        friend class Trie;
    };

    static_assert(std::forward_iterator<Set>);
    static_assert(std::ranges::range<Set>);

    Trie() = default;

    /// Total Node%s in this Trie - including root().
    constexpr size_t size() const noexcept { return size_; }

    /// @name Set Operations
    /// @note All operations do **not** modify the input set(s); they create a **new** Set.
    ///@{

    /// Create a Set wih a *single* @p def%ent: @f$\{def\}@f$.
    [[nodiscard]] Set create(D* def) {
        if (def->tid() == 0) set(def, counter_++);
        auto [i, ins] = roots_.emplace(def, nullptr);
        if (ins) i->second = make_node(nullptr, def);
        return i->second;
    }

    /// Create a PooledSet wih all elements in the given range.
    template<class I> [[nodiscard]] Set create(I begin, I end) {
        if (begin == end) return {};

        Vector<D*> vec(begin, end);
        std::ranges::sort(begin, end);
        Set i = create(*begin);
        for (const auto& def : vec.span().subspan(1)) i = insert(def);
        return i;
    }

    /// Yields @f$a \cup \{def\}@f$.
    constexpr Set insert(Set i, D* def) noexcept {
        if (!i) return create(def);
        if (*i == def) return i;
        if (def->tid() == 0) {
            set(def, counter_++);
            return create(i.node_, def);
        }
        if (i < def) return create(i.node_, def);
        return create(insert(i.parent(), def), *i);
    }

    /// Yields @f$i \setminus def@f$.
    [[nodiscard]] Set erase(Set i, const D* def) {
        if (!i) return {};
        if (*i == def) return i.parent();
        if (def->tid() == 0) return i;
        if (i < def) return i;
        return create(erase(i.parent(), def), *i);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        auto gt = a > b;
        return create(gt ? merge(a.parent(), b) : merge(a, b.parent()), gt ? *a : *b);
    }

    void dot() { dot("trie.dot"); }

    size_t max_depth() {
        size_t res = 0;
        for (auto [_, child] : roots_) res = std::max(res, max_depth(child, 1));
        return res;
    }

    size_t max_depth(const Node* n, size_t depth) {
        size_t res = depth;
        for (auto [_, child] : n->children_) res = std::max(res, max_depth(child, depth + 1));
        return res;
    }

    void dot(std::string s) {
        auto os = std::ofstream(s);
        println(os, "digraph {{");
        println(os, "ordering=out;");
        println(os, "node [shape=box,style=filled];");
        for (auto [_, root] : roots_) root->dot(os);
        println(os, "}}");
    }

    friend void swap(Trie& t1, Trie& t2) noexcept {
        using std::swap;
        // clang-format off
        swap(t1.arena_,   t2.arena_);
        swap(t1.size_,    t2.size_);
        swap(t1.roots_,   t2.roots_);
        swap(t1.counter_, t2.counter_);
        // clang-format on
    }

    static void set(const D* def, u32 tid) { def->tid_ = tid; }

private:
    Node* create(Set s, D* def) { return create(s.node_, def); }
    Node* create(Node* parent, D* def) {
        assert(def->tid() != 0);
        auto& children = parent ? parent->children_ : roots_;
        auto [i, ins]  = children.emplace(def, nullptr);
        if (ins) i->second = make_node(parent, def);
        return i->second;
    }

    Node* make_node(Node* parent, D* def) {
        ++size_;
        auto buff = arena_.allocate<Node>(1);
        auto node = new (buff) Node(parent, def);
        return node;
    }

    fe::Arena arena_;
    size_t size_ = 0;
    GIDMap<D*, Node*> roots_;
    u32 counter_ = 1;
};

} // namespace mim
