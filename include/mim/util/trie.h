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
        constexpr Set(const Trie& trie) noexcept
            : node_(trie.root_.get()) {}
        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr bool empty() const noexcept { return node_ == nullptr || node_->parent_ == nullptr; }
        constexpr Set parent() const noexcept { return node_->parent_; }
        constexpr bool is_root() const noexcept { return parent() == nullptr; }
        constexpr size_t size() const noexcept { return node_ ? node_->size_ : 0; }
        constexpr size_t min() const noexcept { return node_->min_; }
        ///@}

        /// @name Check Membership
        ///@{
        bool contains(const D* def) const noexcept {
            if (this->empty() || def->tid() < this->min() || def->tid() > (**this)->tid()) goto no;
            for (auto i = *this; !i.empty(); ++i)
                if (i == def) goto yes;

no:
            for (auto x : *this)
                if (x == def) assert(false);
            return false;
yes:
            for (auto x : *this)
                if (x == def) return true;
            assert(false);
            return true;
        }

        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            if (this->empty() || other.empty()) goto no;

            for (auto ai = this->begin(), bi = other.begin(); ai && bi;) {
                if (*ai == *bi) goto yes;

                if ((*ai)->tid() > (*bi)->tid())
                    ++ai;
                else
                    ++bi;
            }

no:
            for (auto x : *this) assert(!other.contains(x));
            return false;

yes:
            for (auto x : *this)
                if (other.contains(x)) return true;
            assert(false);
            return true;
        }
        ///@}

        void dump() const {
            std::cout << '{';
            for (auto sep = ""; auto i : *this) {
                std::cout << sep << i->tid();
                sep = ", ";
            }
            std::cout << '}' << std::endl;
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

    Trie()
        : root_(make_node(nullptr, 0)) {}

    constexpr const Node* root() const noexcept { return root_; }
    /// Total Node%s in this Trie - including root().
    constexpr size_t size() const noexcept { return size_; }

    /// @name Set Operations
    /// @note All operations do **not** modify the input set(s); they create a **new** Set.
    ///@{
    [[nodiscard]] Set create() { return root_.get(); }

    /// Create a Set wih a *single* @p def%ent: @f$\{def\}@f$.
    [[nodiscard]] Set create(D* def) {
        if (def->tid() == 0) set(def, counter_++);
        return create(root_.get(), def);
    }

    /// Create a PooledSet wih all defents in the given range.
    template<class I> [[nodiscard]] Set create(I begin, I end) {
        Vector<D*> vec(begin, end);
        // std::ranges::sort(begin, end);
        Set i = root();
        for (const auto& def : vec) i = insert(def);

        for (auto x : vec) assert(i.contains(x));
        return i;
    }

    /// Yields @f$a \cup \{def\}@f$.
    constexpr Set insert(Set i, D* def) noexcept {
        if (i.empty()) {
            if (def->tid() == 0) set(def, counter_++);
            return create(root_.get(), def);
        }
        if (*i == def) return i;
        if (i.is_root()) {
            if (def->tid() == 0) set(def, counter_++);
            Set res = create(i.node_, def);
            assert(res.contains(def));
            return res;
        }
        if (def->tid() == 0) {
            set(def, counter_++);
            Set res = create(i.node_, def);
            assert(res.contains(def));
            return res;
        }
        if (i < def) {
            Set res = create(i.node_, def);
            assert(res.contains(def));
            return res;
        }
        Set res = create(insert(i.parent(), def), *i);
        assert(res.contains(def));
        return res;
    }

    /// Yields @f$i \setminus def@f$.
    [[nodiscard]] Set erase(Set i, const D* def) {
        if (def->tid() == 0) {
            assert(!i.contains(def));
            return i;
        }
        if (*i == def) {
            Set res = i.parent();
            assert(!res.contains(def));
            return res;
        }
        if (i < def) {
            Set res = i;
            assert(!res.contains(def));
            return res;
        }
        Set res = create(erase(i.parent(), def), *i);
        assert(!res.contains(def));
        return res;
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        Set res;
        if ((*a)->tid() == (*b)->tid())
            res = create(merge(a.parent(), b.parent()), *a);
        else if ((*a)->tid() < (*b)->tid())
            res = create(merge(a, b.parent()), *b);
        else
            res = create(merge(a.parent(), b), *a);

        for (auto x : a) assert(res.contains(x));
        for (auto x : b) assert(res.contains(x));
        return res;
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
        swap(t1.size_,    t2.size_);
        swap(t1.root_,    t2.root_);
        swap(t1.counter_, t2.counter_);
        // clang-format on
    }

    static void set(const D* def, u32 tid) { def->tid_ = tid; }

private:
    Node* create(Set parent, D* def) {
        assert(def->tid() != 0);
        auto [i, ins] = parent.node_->children_.emplace(def, nullptr);
        if (ins) i->second = make_node(parent.node_, def);
        return i->second.get();
    }

    fe::Arena::Ptr<Node> make_node(Node* parent, D* def) {
        ++size_;
        return arena_.mk<Node>(parent, def);
    }

    fe::Arena arena_;
    size_t size_ = 0;
    u32 counter_ = 1;
    fe::Arena::Ptr<Node> root_;
};

} // namespace mim
