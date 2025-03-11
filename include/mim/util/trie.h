#pragma once

#include <compare>

#include <fstream>

#include <fe/arena.h>

#include "mim/util/print.h"
#include "mim/util/util.h"
#include "mim/util/vector.h"

#include "absl/container/flat_hash_map.h"

namespace mim {

template<class T> class Trie {
public:
    class Node {
    public:
        Node(Node* parent, T elem)
            : elem_(elem)
            , parent_(parent)
            , size_(parent ? parent->size_ + 1 : 0) {}

        void dot(std::ostream& os) {
            for (auto [elem, child] : children_) {
                println(os, "n{}_{} -> n{}_{}", elem_ ? elem_->lid() : u32(0xffff), this, child->elem_->lid(), child);
                child->dot(os);
            }
        }

    private:
        T elem_;
        Node* parent_;
        size_t size_;

        struct LIDEq {
            constexpr bool operator()(const T& a, const T& b) const noexcept { return a->lid_ == b->lid_; }
        };

        struct LIDHash {
            constexpr size_t operator()(const T& a) const noexcept { return hash(a->lid_); }
        };

        absl::flat_hash_map<T, Node*, LIDHash, LIDEq> children_;

        friend class Trie;
    };

    /// Also serves as iterator.
    /// `set++` will remove the last element from the set by moving the internal Node pointer one parent up.
    class Set {
    public:
        /// @name Iterator Properties
        ///@{
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        ///@}

        /// @name Construction
        ///@{
        constexpr Set() noexcept           = default;
        constexpr Set(const Set&) noexcept = default;
        constexpr Set(Set&&) noexcept      = default;
        constexpr Set(Node* node) noexcept
            : node_(node) {}
        constexpr Set(const Trie& trie) noexcept
            : node_(trie.root_) {}
        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr bool empty() const noexcept { return node_ == nullptr || node_->parent_ == nullptr; }
        constexpr Set parent() const noexcept { return node_->parent_; }
        constexpr bool is_root() const noexcept { return parent() == nullptr; }
        constexpr size_t size() const noexcept { return node_->size_; }
        constexpr bool contains(const T& elem) const noexcept {
            for (auto i = *this; !i.empty(); ++i)
                if (i == elem) return true;
            return false;
        }
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
        constexpr bool operator==(const T& other) const noexcept { return !this->empty() && this->node_->elem_ == other; }
        constexpr bool operator!=(const T& other) const noexcept { return  this->empty() || this->node_->elem_ != other; }
        // clang-format on
        constexpr auto operator<=>(Set other) const noexcept {
            if (this->empty() && other.empty()) return std::strong_ordering::equal;
            if (this->empty()) return std::strong_ordering::less;
            if (other.empty()) return std::strong_ordering::greater;
            return (**this)->lid() <=> (*other)->lid();
        }
        constexpr auto operator<=>(const T& other) const noexcept {
            if (this->empty()) return std::strong_ordering::less;
            return (**this)->lid() <=> other->lid();
        }
        ///@}

        /// @name Conversions
        ///@{
        constexpr explicit operator bool() const noexcept { return !empty(); } ///< Is not empty?
        constexpr reference operator*() const noexcept { return node_->elem_; }
        constexpr pointer operator->() const noexcept { return node_->elem_; }
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

    Trie()
        : root_(make_node(nullptr, 0)) {}

    constexpr const Node* root() const noexcept { return root_; }
    /// Total Node%s in this Trie - including root().
    constexpr size_t size() const noexcept { return size_; }

    /// @name Set Operations
    /// @note All operations do **not** modify the input set(s); they create a **new** Set.
    ///@{
    [[nodiscard]] Set create() { return root_; }

    /// Create a Set wih a *single* @p elem%ent: @f$\{elem\}@f$.
    [[nodiscard]] Set create(const T& elem) {
        if (elem->lid_ == 0) elem->lid_ = counter_++;
        return create(root_, elem);
    }

    /// Create a PooledSet wih all elements in the given range.
    template<class I> [[nodiscard]] Set create(I begin, I end) {
        Vector<const T> vec(begin, end);
        std::ranges::sort(begin, end);
        Set i = root();
        for (const auto& elem : vec) i = insert(elem);
        return i;
    }

    /// Yields @f$a \cup \{elem\}@f$.
    constexpr Set insert(Set i, const T& elem) noexcept {
        if (*i == elem) return i;
        if (i.is_root()) {
            if (elem->lid_ == 0) elem->lid_ = counter_++;
            return create(i.node_, elem);
        }
        if (elem->lid_ == 0) {
            elem->lid_ = counter_++;
            return create(i.node_, elem);
        }
        if (i < elem) return create(i.node_, elem);
        return create(insert(i.parent(), elem), *i);
    }

    /// Yields @f$i \setminus elem@f$.
    [[nodiscard]] Set erase(Set i, const T& elem) {
        if (elem->lid_ == 0) return i;
        if (*i == elem) return i.parent();
        if (i < elem) return i;
        return create(erase(i.parent(), elem), *i);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        auto gt = a > b;
        return create(gt ? merge(a.parent(), b) : merge(a, b.parent()), gt ? *a : *b);
    }

    void dot() { dot("trie.dot"); }

    size_t max_depth() { return max_depth(root_, 0); }

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
        root_->dot(os);
        println(os, "}}");
    }

    friend void swap(Trie& t1, Trie& t2) noexcept {
        using std::swap;
        swap(t1.arena_, t2.arena_);
        swap(t1.size_, t2.size_);
        swap(t1.root_, t2.root_);
        swap(t1.counter_, t2.counter_);
    }

private:
    Node* create(Set parent, const T& elem) {
        assert(elem->lid_ != 0);
        auto [i, ins] = parent.node_->children_.emplace(elem, nullptr);
        if (ins) i->second = make_node(parent.node_, elem);
        return i->second;
    }

    Node* make_node(Node* parent, const T& elem) {
        ++size_;
        auto buff = arena_.allocate(sizeof(Node));
        auto node = new (buff) Node(parent, elem);
        return node;
    }

#if 0
    std::pair<Set, bool> find(Set i, u32 elem) {
        auto j = i;
        for (; *j >= elem; ++j)
            if (*j == elem) return {j, true};
        return {j, false};
    }
#endif

    fe::Arena arena_;
    size_t size_ = 0;
    Node* root_;
    u32 counter_ = 1;

    friend class Range;
};

} // namespace mim
