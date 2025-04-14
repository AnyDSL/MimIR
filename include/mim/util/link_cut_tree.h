#pragma once

#include <cstddef>

namespace mim::lct {

/// This is an **intrusive** [Link-Cut-Tree](https://en.wikipedia.org/wiki/Link/cut_tree).
/// Intrusive means that you have to inherit from this class via
/// [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) like this:
/// ```
/// class Node : public lct::Node<Node, MyKey> {
///     constexpr bool lt(const MyKey& key) const noexcept { /*...*/ }
///     constexpr bool eq(const MyKey& key) const noexcept { /*...*/ }
///     // ...
/// };
/// ```
template<class P, class K> class Node {
private:
    P* self() { return static_cast<P*>(this); }
    const P* self() const { return static_cast<const P*>(this); }

public:
    constexpr Node() noexcept = default;

    ///@name Getters
    ///@{
    [[nodiscard]] bool contains(const K& k) noexcept {
        expose();
        for (auto n = this; n; n = n->self()->lt(k) ? n->bot : n->top)
            if (n->self()->eq(k)) return n->splay(), true;

        return false;
    }

    /// Find @p k or the element just greater than @p k.
    constexpr P* find(const K& k) noexcept {
        expose();
        auto prev = this;
        for (auto n = this; n;) {
            if (n->self()->eq(k)) return n->splay(), n->self();

            if (n->self()->lt(k)) {
                n = n->bot;
            } else {
                prev = n;
                n    = n->top;
            }
        }

        return prev->self();
    }
    ///@}

    ///@name parent
    ///@{
    // clang-format off
    constexpr Node*  aux_parent() noexcept { return parent && (parent->bot == this || parent->top == this) ? parent         : nullptr; }
    constexpr P*    path_parent() noexcept { return parent && (parent->bot != this && parent->top != this) ? parent->self() : nullptr; }
    // clang-format on
    ///@}

    ///@name Splay Tree
    ///@{

    /// [Splays](https://hackmd.io/@CharlieChuang/By-UlEPFS#Operation1) `this` to the root of its splay tree.
    constexpr void splay() noexcept {
        while (auto p = aux_parent()) {
            if (auto pp = p->aux_parent()) {
                if (p->bot == this && pp->bot == p) { // zig-zig
                    pp->ror();
                    p->ror();
                } else if (p->top == this && pp->top == p) { // zag-zag
                    pp->rol();
                    p->rol();
                } else if (p->bot == this && pp->top == p) { // zig-zag
                    p->ror();
                    pp->rol();
                } else { // zag-zig
                    assert(p->top == this && pp->bot == p);
                    p->rol();
                    pp->ror();
                }
            } else if (p->bot == this) { // zig
                p->ror();
            } else { // zag
                assert(p->top == this);
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
    template<size_t l> constexpr void rotate() noexcept {
        constexpr size_t r   = (l + 1) % 2;
        constexpr auto child = [](Node* n, size_t i) -> Node*& { return i == 0 ? n->bot : n->top; };

        auto x = this;
        auto p = x->parent;
        auto c = child(x, r);
        auto b = child(c, l);

        if (b) b->parent = x;

        if (p) {
            if (child(p, l) == x) {
                child(p, l) = c;
            } else if (child(p, r) == x) {
                child(p, r) = c;
            } else {
                /* only path parent */;
            }
        }

        x->parent   = c;
        c->parent   = p;
        child(x, r) = b;
        child(c, l) = x;
    }

    constexpr void rol() noexcept { return rotate<0>(); }
    constexpr void ror() noexcept { return rotate<1>(); }
    ///@}

    /// @name Link-Cut-Tree
    ///@{

    /// Registers the edge `this -> child` in the *aux* tree.
    constexpr void link(Node* child) noexcept {
        this->expose();
        child->expose();
        if (!child->top) {
            this->parent = child;
            child->top   = this;
        }
    }

    /// Make a preferred path from `this` to root while putting `this` at the root of the *aux* tree.
    /// @returns the last valid path_parent().
    constexpr P* expose() noexcept {
        Node* prev = nullptr;
        for (auto curr = this; curr; prev = curr, curr = curr->parent) {
            curr->splay();
            assert(!prev || prev->parent == curr);
            curr->bot = prev;
        }
        splay();
        return prev->self();
    }

    /// Least Common Ancestor of `this` and @p other in the *aux* tree; leaves @p other expose%d.
    /// @returns `nullptr`, if @p a and @p b are in different trees.
    constexpr P* lca(Node* other) noexcept { return this->expose(), other->expose(); }

    /// Is `this` a descendant of `other` in the *aux* tree?
    /// Also `true`, if `this == other`.
    constexpr bool is_descendant_of(Node* other) noexcept {
        if (this == other) return true;
        this->expose();
        other->splay();
        auto curr = this;
        while (auto p = curr->aux_parent()) curr = p;
        return curr == other;
    }
    ///@}

    Node* parent = nullptr; ///< parent or path-parent
    Node* bot    = nullptr; ///< left/deeper/bottom/leaf-direction
    Node* top    = nullptr; ///< right/shallower/top/root-direction
};

} // namespace mim::lct
