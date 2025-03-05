#pragma once

#include <compare>

#include <fstream>
#include <new>

#include <fe/arena.h>

#include "mim/util/print.h"
#include "mim/util/util.h"

#include "absl/container/flat_hash_map.h"
#include "fe/assert.h"

namespace mim {

template<class T> class Trie {
public:
    class Node {
    public:
        Node(Node* parent, const T* elem)
            : elem_(elem)
            , parent_(parent)
            , size_(parent ? parent->size_ + 1 : 0) {}

        void dot(std::ostream& os) {
            for (auto [elem, child] : children_) {
                println(os, "n{}_{} -> n{}_{}", elem_ ? elem_->gid() : u32(0xffff), this, child->elem_->gid(), child);
                child->dot(os);
            }
        }

    private:
        const T* elem_;
        Node* parent_;
        size_t size_;
        GIDMap<const T*, Node*> children_;

        friend class Trie;
    };

    /// Also serves as iterator.
    /// `set++` will remove the last element from the set by moving the internal Node pointer one parent up.
    class Set {
    public:
        /// @name Constructors
        ///@{
        constexpr Set(const Set&) noexcept = default;
        constexpr Set(Set&&) noexcept      = default;
        constexpr Set(Node* node) noexcept
            : node_(node) {}
        ///@}

        /// @name Getters
        ///@{
        constexpr Set parent() const noexcept { return node_->parent_; }
        constexpr bool is_root() const noexcept { return parent() == nullptr; }
        constexpr bool empty() const noexcept { return is_root(); }
        constexpr size_t size() const noexcept { return node_->size_; }
        ///@}

        /// @name Comparisons
        ///@{
        constexpr bool operator==(Set other) const noexcept { return this->node_ == other.node_; }
        constexpr bool operator!=(Set other) const noexcept { return this->node_ != other.node_; }
        constexpr auto operator<=>(Set other) const noexcept {
            if (this->is_root() && other.is_root()) return std::strong_ordering::equal;
            if (this->is_root()) return std::strong_ordering::less;
            if (other.is_root()) return std::strong_ordering::greater;
            return (*this)->gid() <=> other->gid();
        }
        constexpr auto operator<=>(const T* other) const noexcept {
            if (this->is_root()) return std::strong_ordering::less;
            return (*this)->gid() <=> other->gid();
        }
        ///@}

        /// @name Conversions
        ///@{
        constexpr explicit operator bool() const noexcept { return !empty(); } ///< Is not empty?
        constexpr const T* operator*() const noexcept { return node_->elem_; }
        constexpr const T* operator->() const noexcept { return node_->elem_; }
        ///@}

        /// @name Increment
        ///@{
        constexpr Set operator++(int) noexcept {
            auto res = *this;
            node_    = node_->parent_;
            return res;
        }
        constexpr Set& operator++() noexcept { return node_ = node_->parent_, *this; }
        ///@}

    private:
        Node* node_;

        friend class Trie;
    };

    class Range {
    public:
        constexpr Range(const Trie& trie, Set begin) noexcept
            : trie_(trie)
            , begin_(begin) {}

        constexpr const Trie& trie() const noexcept { return trie_; }

        /// @name Iterators
        ///@{
        constexpr Set begin() const noexcept { return begin_; }
        constexpr Set end() const noexcept { return trie().root_; }
        constexpr Set cbegin() const noexcept { return begin(); }
        constexpr Set cend() const noexcept { return end(); }
        ///@}

    private:
        const Trie& trie_;
        Set begin_;
    };

    Trie()
        : root_(make_node(nullptr, 0)) {}

    constexpr const Node* root() const noexcept { return root_; }

    Range range(Set i) { return {*this, i.node_}; }

    /// @name Set Operations
    /// @note All operations do **not** modify the input set(s); they create a **new** Set.
    ///@{

    /// Create a Set wih a *single* @p elem%ent: @f$\{elem\}@f$.
    [[nodiscard]] Set create(const T* elem) { return create(root_, elem); }

    /// Create a PooledSet wih all elements in the given range.
    template<class I> [[nodiscard]] Set create(I, I) {
        // TODO
        return nullptr;
    }

    /// Yields @f$a \cup \{elem\}@f$.
    constexpr Set insert(Set i, const T* elem) noexcept {
        if (*i == elem) return i;
        if (i < elem) return create(i.node_, elem);
        return create(insert(i.parent(), elem), *i);
    }

    /// Yields @f$i \setminus elem@f$.
    [[nodiscard]] Set erase(Set i, const T* elem) {
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

    void dot() {
        auto os = std::ofstream("out.dot");
        println(os, "digraph {{");
        println(os, "ordering=out;");
        println(os, "node [shape=box,style=filled];");
        root_->dot(os);
        println(os, "}}");
    }

    friend void swap(Trie& t1, Trie& t2) noexcept {
        using std::swap;
        swap(t1.arena_, t2.arena_);
        swap(t1.root_, t2.root_);
    }

private:
    Node* create(Set parent, const T* elem) {
        auto [i, ins] = parent.node_->children_.emplace(elem, nullptr);
        if (ins) i->second = make_node(parent.node_, elem);
        return i->second;
    }

    Node* make_node(Node* parent, const T* elem) {
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
    Node* root_;

    friend class Range;
};

} // namespace mim
