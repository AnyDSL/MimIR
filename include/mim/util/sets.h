#pragma once

#include "mim/util/util.h"
#include "mim/util/vector.h"

#include "fe/arena.h"

namespace mim {

template<class D, size_t N = 16> class Sets {
private:
    /// Trie Node.
    class Node {
    public:
        constexpr Node(u32 id) noexcept
            : def(nullptr)
            , parent(nullptr)
            , size(0)
            , min(size_t(-1))
            , id(id) {}

        constexpr Node(Node* parent, D* def, u32 id) noexcept
            : def(def)
            , parent(parent)
            , size(parent->size + 1)
            , min(parent->def ? parent->min : def->tid())
            , id(id) {
            parent->link_to_child(this);
        }

        void dot(std::ostream& os) const {
            using namespace std::string_literals;

            auto node2str = [](const Node* n) {
                return "n_" + (n->def ? std::to_string(n->def->tid()) : "root"s) + "_"s + std::to_string(n->id);
            };

            println(os, "{} [tooltip=\"min: {}, max: {}\"];", node2str(this), aux.min, aux.max);

            for (const auto& [def, child] : children) println(os, "{} -> {}", node2str(this), node2str(child.get()));

            for (const auto& [_, child] : children) child->dot(os);

            // clang-format off
            if (auto par = path_parent()) println(os, "{} -> {} [constraint=false,color=\"#0000ff\",style=dashed];", node2str(this), node2str(par));
            if (auto top = aux.top      ) println(os, "{} -> {} [constraint=false,color=\"#ff0000\"];", node2str(this), node2str(top));
            if (auto bot = aux.bot      ) println(os, "{} -> {} [constraint=false,color=\"#00ff00\"];", node2str(this), node2str(bot));
            // clang-format on
        }

        ///@name Getters
        ///@{
        constexpr bool is_root() const noexcept { return def == 0; }

        [[nodiscard]] bool contains(D* d) noexcept {
            auto tid = d->tid();
            // clang-format off
            if (tid == this->min || tid == this->def->tid()) return true;
            if (tid <  this->min || tid >  this->def->tid()) return false;
            // clang-format on

            expose();
            for (auto n = this; n; n = n->is_root() ? n->aux.bot : (tid < n->def->tid() ? n->aux.top : n->aux.bot)) {
                if (n->def == d) {
                    n->splay(); // heuristic: bring to root to have hits at the top
                    return true;
                }
            }

            return false;
        }
        ///@}

        ///@name parent
        ///@{
        // clang-format off
        constexpr Node*  aux_parent() noexcept { return aux.parent && (aux.parent->aux.bot == this || aux.parent->aux.top == this) ? aux.parent : nullptr; }
        constexpr Node* path_parent() noexcept { return aux.parent && (aux.parent->aux.bot != this && aux.parent->aux.top != this) ? aux.parent : nullptr; }
        // clang-format on
        ///@}

        ///@name Aux Tree (Splay Tree)
        ///@{

        /// [Splays](https://hackmd.io/@CharlieChuang/By-UlEPFS#Operation1) `this` to the root of its splay tree.
        constexpr void splay() noexcept {
            while (auto p = aux_parent()) {
                if (auto pp = p->aux_parent()) {
                    if (p->aux.bot == this && pp->aux.bot == p) { // zig-zig
                        pp->ror();
                        p->ror();
                    } else if (p->aux.top == this && pp->aux.top == p) { // zag-zag
                        pp->rol();
                        p->rol();
                    } else if (p->aux.bot == this && pp->aux.top == p) { // zig-zag
                        p->ror();
                        pp->rol();
                    } else { // zag-zig
                        assert(p->aux.top == this && pp->aux.bot == p);
                        p->rol();
                        pp->ror();
                    }
                } else if (p->aux.bot == this) { // zig
                    p->ror();
                } else { // zag
                    assert(p->aux.top == this);
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
            constexpr auto child = [](Node* n, size_t i) -> Node*& { return i == 0 ? n->aux.bot : n->aux.top; };

            auto x = this;
            auto p = x->aux.parent;
            auto c = child(x, r);
            auto b = child(c, l);

            if (b) b->aux.parent = x;

            if (p) {
                if (child(p, l) == x) {
                    child(p, l) = c;
                } else if (child(p, r) == x) {
                    child(p, r) = c;
                } else {
                    /* only path parent */;
                }
            }

            x->aux.parent = c;
            c->aux.parent = p;
            child(x, r)   = b;
            child(c, l)   = x;
        }

        constexpr void rol() noexcept { return rotate<0>(); }
        constexpr void ror() noexcept { return rotate<1>(); }
        ///@}

        /// @name Link-Cut-Tree
        /// This is a simplified version of a Link-Cut-Tree without the Cut operation.
        ///@{

        /// Registers the edge `this -> child` in the *aux* tree.
        constexpr void link_to_child(Node* child) noexcept {
            this->expose();
            child->expose();
            if (!child->aux.top) {
                this->aux.parent = child;
                child->aux.top   = this;
            }
        }

        /// Make a preferred path from `this` to root while putting `this` at the root of the *aux* tree.
        /// @returns the last valid path_parent().
        constexpr Node* expose() noexcept {
            Node* prev = nullptr;
            for (auto curr = this; curr; prev = curr, curr = curr->aux.parent) {
                curr->splay();
                assert(!prev || prev->aux.parent == curr);
                curr->aux.bot = prev;
            }
            splay();
            return prev;
        }

        /// Least Common Ancestor of `this` and @p other in the *rep* tree; leaves @p other expose%d.
        /// @returns `nullptr`, if @p a and @p b are in different trees.
        constexpr Node* lca(Node* other) noexcept { return this->expose(), other->expose(); }

        /// Is `this` a descendant of `other` in the *rep* tree?
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

        D* const def;
        Node* const parent;
        const size_t size;
        const size_t min;
        u32 const id;
        GIDMap<D*, fe::Arena::Ptr<Node>> children;

        struct {
            Node* parent = nullptr; ///< parent or path-parent
            Node* bot    = nullptr; ///< left/deeper/bottom/leaf-direction
            Node* top    = nullptr; ///< right/shallower/top/root-direction
        } aux;
    };

    struct Data {
        constexpr Data() noexcept = default;
        constexpr Data(size_t size) noexcept
            : size(size) {}

        size_t size = 0;
        D* elems[];

        struct Equal {
            constexpr bool operator()(const Data* d1, const Data* d2) const noexcept {
                bool res = d1->size == d2->size;
                for (size_t i = 0, e = d1->size; res && i != e; ++i) res &= d1->elems[i] == d2->elems[i];
                return res;
            }
        };

        /// @name Iterators
        ///@{
        constexpr D** begin() noexcept { return elems; }
        constexpr D** end() noexcept { return elems + size; }
        constexpr D* const* begin() const noexcept { return elems; }
        constexpr D* const* end() const noexcept { return elems + size; }
        ///@}

        template<class H> friend constexpr H AbslHashValue(H h, const Data* d) noexcept {
            if (!d) return H::combine(std::move(h), 0);
            return H::combine_contiguous(std::move(h), d->elems, d->size);
        }
    };

public:
    class Set {
    private:
        enum class Tag : uintptr_t { Null, Uniq, Data, Node };

        constexpr Set(const Data* data) noexcept
            : ptr_(uintptr_t(data) | uintptr_t(Tag::Data)) {} ///< Data Set.
        constexpr Set(Node* node) noexcept
            : ptr_(uintptr_t(node) | uintptr_t(Tag::Node)) {} ///< Node set.

    public:
        class iterator {
        private:
            constexpr iterator(D* d) noexcept
                : tag_(Tag::Uniq)
                , ptr_(std::bit_cast<uintptr_t>(d)) {}
            constexpr iterator(D* const* elems) noexcept
                : tag_(Tag::Data)
                , ptr_(std::bit_cast<uintptr_t>(elems)) {}
            constexpr iterator(Node* node) noexcept
                : tag_(Tag::Node)
                , ptr_(std::bit_cast<uintptr_t>(node)) {}

        public:
            /// @name Iterator Properties
            ///@{
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = D*;
            using pointer           = D* const*;
            using reference         = D* const&;
            ///@}

            /// @name Construction
            ///@{
            constexpr iterator() noexcept = default;
            ///@}

            /// @name Increment
            /// @note These operations only change the *view* of this Set; the Set itself is **not** modified.
            ///@{
            constexpr iterator& operator++() noexcept {
                // clang-format off
                switch (tag_) {
                    case Tag::Uniq: return clear();
                    case Tag::Data: return ptr_ = std::bit_cast<uintptr_t>(std::bit_cast<D* const*>(ptr_) + 1), *this;
                    case Tag::Node: {
                        auto node = std::bit_cast<Node*>(ptr_);
                        node      = node->parent;
                        if (node->is_root())
                            clear();
                        else
                            ptr_ = std::bit_cast<uintptr_t>(node);
                        return *this;
                    }
                    default: fe::unreachable();
                }
                // clang-format on
            }

            constexpr iterator operator++(int) noexcept {
                auto res = *this;
                this->operator++();
                return res;
            }
            ///@}

            /// @name Comparisons
            ///@{
            // clang-format off
            constexpr bool operator==(iterator other) const noexcept { return this->tag_ == other.tag_ && this->ptr_ == other.ptr_; }
            constexpr bool operator!=(iterator other) const noexcept { return this->tag_ != other.tag_ || this->ptr_ != other.ptr_; }
            // clang-format on
            ///@}

            /// @name Dereference
            ///@{
            constexpr reference operator*() const noexcept {
                switch (tag_) {
                    case Tag::Uniq: return *std::bit_cast<D* const*>(&ptr_);
                    case Tag::Data: return *std::bit_cast<D* const*>(ptr_);
                    case Tag::Node: return std::bit_cast<Node*>(ptr_)->def;
                    default: fe::unreachable();
                }
            }

            constexpr pointer operator->() const noexcept { return this->operator*(); }
            ///@}

            iterator& clear() { return tag_ = Tag::Null, ptr_ = 0, *this; }

        private:
            Tag tag_;
            uintptr_t ptr_;

            friend class Set;
        };

        /// @name Construction
        ///@{
        constexpr Set(const Set&) noexcept = default;
        constexpr Set(Set&&) noexcept      = default;
        constexpr Set() noexcept           = default; ///< Null set
        constexpr Set(D* d) noexcept
            : ptr_(uintptr_t(d) | uintptr_t(Tag::Uniq)) {} ///< Uniq set.

        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr size_t size() const noexcept {
            if (isa_uniq()) return 1;
            if (auto d = isa_data()) return d->size;
            if (auto n = isa_node()) return n->size;
            return 0; // empty
        }

        /// Is empty?
        constexpr bool empty() const noexcept {
            assert(tag() != Tag::Node || !ptr<Node>()->is_root());
            return ptr_ == 0;
        }

        constexpr explicit operator bool() const noexcept { return ptr_ != 0; } ///< Not empty?
        ///@}

        /// @name Check Membership
        ///@{

        /// Is @f$d \in this@f$?.
        bool contains(D* d) const noexcept {
            if (auto u = isa_uniq()) return d == u;

            if (auto data = isa_data()) {
                for (auto e : *data)
                    if (d == e) return true;
                return false;
            }

            if (auto n = isa_node()) return n->contains(d);

            return false;
        }

        /// Is @f$this \cap other \neq \emptyset@f$?.
        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            if (this->empty() || other.empty()) return false;
            if (auto u = this->isa_uniq()) return other.contains(u);
            if (auto u = other.isa_uniq()) return this->contains(u);

            auto d1 = this->isa_data(), d2 = other.isa_data();
            auto n1 = this->isa_node(), n2 = other.isa_node();

            if (d1 && d2) {
                if (d1 == d2) return true;

                for (auto ai = d1->begin(), ae = d1->end(), bi = d2->begin(), be = d2->end(); ai != ae && bi != be;) {
                    if (*ai == *bi) return true;

                    if ((*ai)->gid() < (*bi)->gid())
                        ++ai;
                    else
                        ++bi;
                }

                return false;
            }

            if (n1 && n2) {
                if (!n1->lca(n2)->is_root()) return true;
                if (n1->min > n2->def->tid() || n1->def->tid() < n2->min) return false;

                // if one set is way smaller, iterate over this one and check with `contains` the other one
                if (auto n1_lt = n1->size <= n2->size >> 3; n1_lt || n1->size <= n2->size >> 3) {
                    auto m = n1_lt ? n2 : n1;
                    for (auto n = n1_lt ? n1 : n2; !n->is_root(); n = n->parent)
                        if (m->contains(n->def)) return true;
                    return false;
                }

                while (!n1->is_root() && !n2->is_root()) {
                    if (n1->def == n2->def) {
                        n1->splay();
                        n2->splay();
                        return true;
                    }

                    if (n1->def->tid() > n2->def->tid()) {
                        n1 = n1->parent;
                        if (n1->def->tid() < n2->min) return false;
                    } else {
                        n2 = n2->parent;
                        if (n1->min > n2->def->tid()) return false;
                    }
                }

                return false;
            }

            auto n = n1 ? n1 : n2;
            for (auto e : *(d1 ? d1 : d2))
                if (n->contains(e)) return true;

            return false;
        }
        ///@}

        /// @name Iterators
        ///@{
        constexpr iterator begin() const noexcept {
            if (auto u = isa_uniq()) return {u};
            if (auto d = isa_data()) return {d->begin()};
            if (auto n = isa_node(); n && !n->is_root()) return {n};
            return {};
        }

        constexpr iterator end() const noexcept {
            if (auto data = isa_data()) return iterator(data->end());
            return {};
        }
        ///@}

        /// @name Comparisons
        ///@{
        constexpr bool operator==(Set other) const noexcept { return this->ptr_ == other.ptr_; }
        constexpr bool operator!=(Set other) const noexcept { return this->ptr_ != other.ptr_; }
        ///@}

        /// @name Output
        ///@{
        std::ostream& stream(std::ostream& os) const {
            os << size() << " - {";
            auto sep = "";
            for (auto d : *this) {
                os << sep << d->gid() << '/' << d->tid();
                sep = ", ";
            }
            return os << '}' << std::endl;
        }

        void dump() const { stream(std::cout) << std::endl; }
        ///@}

    private:
        constexpr Tag tag() const noexcept { return Tag(ptr_ & uintptr_t(0b11)); }
        template<class T> constexpr T* ptr() const noexcept {
            return std::bit_cast<T*>(ptr_ & (uintptr_t(-2) << uintptr_t(2)));
        }
        // clang-format off
        constexpr D*    isa_uniq() const noexcept { return tag() == Tag::Uniq ? ptr<D   >() : nullptr; }
        constexpr Data* isa_data() const noexcept { return tag() == Tag::Data ? ptr<Data>() : nullptr; }
        constexpr Node* isa_node() const noexcept { return tag() == Tag::Node ? ptr<Node>() : nullptr; }
        // clang-format on

        uintptr_t ptr_ = 0;

        friend class Sets;
        friend std::ostream& operator<<(std::ostream& os, Set set) { return set.stream(os); }
    };

    static_assert(std::forward_iterator<typename Set::iterator>);
    static_assert(std::ranges::range<Set>);

    /// @name Construction
    ///@{
    Sets& operator=(const Sets&) = delete;

    constexpr Sets() noexcept
        : root_(make_node()) {}
    constexpr Sets(const Sets&) noexcept = delete;
    constexpr Sets(Sets&& other) noexcept
        : Sets() {
        swap(*this, other);
    }
    ///@}

    /// @name Set Operations
    /// @note These operations do **not** modify the input set(s); they create a **new** Set.
    ///@{

    /// Create a Set wih all elements in @p v.
    [[nodiscard]] Set create(Vector<D*> v) {
        auto vb = v.begin();
        auto ve = v.end();
        std::sort(vb, ve, [](D* d1, D* d2) { return d1->gid() < d2->gid(); });
        auto vu   = std::unique(vb, ve);
        auto size = std::distance(vb, vu);

        if (size == 0) return {};
        if (size == 1) return {*vb};

        if (size_t(size) <= N) {
            auto [data, state] = allocate(size);
            std::copy(vb, vu, data->begin());
            return unify(data, state);
        }

        // sort in ascending tids but 0 goes last
        std::sort(vb, vu, [](D* d1, D* d2) { return d1->tid() != 0 && (d2->tid() == 0 || d1->tid() < d2->tid()); });

        auto res = root();
        for (auto i = vb; i != vu; ++i) res = insert(res, *i);
        return res;
    }

    /// Yields @f$s \cup d@f$.
    [[nodiscard]] Set insert(Set s, D* d) {
        if (auto u = s.isa_uniq()) {
            if (d == u) return {d};

            auto [data, state] = allocate(2);
            if (d->gid() < u->gid())
                data->elems[0] = d, data->elems[1] = u;
            else
                data->elems[0] = u, data->elems[1] = d;
            return unify(data, state);
        }

        if (auto src = s.isa_data()) {
            auto size = src->size;
            if (size + 1 <= N) {
                auto [dst, state] = allocate(size + 1);

                // copy over and insert new element d
                bool ins = false;
                for (auto si = src->begin(), di = dst->begin(), se = src->end(); si != se || !ins; ++di) {
                    if (si != se && d == *si) { // already here
                        data_arena_.deallocate(state);
                        return s;
                    }

                    if (!ins && (si == se || d->gid() < (*si)->gid()))
                        *di = d, ins = true;
                    else
                        *di = *si++;
                }

                return unify(dst, state);
            } else { // we need to switch from Data to Node
                auto [dst, state] = allocate(size + 1);

                // copy over
                auto di = dst->begin();
                for (auto si = src->begin(), se = src->end(); si != se; ++si, ++di) {
                    if (d == *si) { // already here
                        data_arena_.deallocate(state);
                        return s;
                    }

                    *di = *si;
                }
                *di = d; // put new element at last into dst->elems

                // sort in ascending tids but 0 goes last
                std::sort(dst->begin(), di,
                          [](D* d1, D* d2) { return d1->tid() != 0 && (d2->tid() == 0 || d1->tid() < d2->tid()); });

                auto res = root();
                for (auto i = dst->begin(), e = dst->end(); i != e; ++i) res = insert(res, *i);
                return res;
            }
        }

        if (auto n = s.isa_node()) {
            if (n->contains(d)) return n;
            return insert(n, d);
        }

        return {d};
    }

    /// Yields @f$s_1 \cup s_2@f$.
    [[nodiscard]] Set merge(Set s1, Set s2) {
        if (s1.empty() || s1 == s2) return s2;
        if (s2.empty()) return s1;

        if (auto u = s1.isa_uniq()) return insert(s2, u);
        if (auto u = s2.isa_uniq()) return insert(s1, u);

        auto d1 = s1.isa_data(), d2 = s2.isa_data();
        auto n1 = s1.isa_node(), n2 = s2.isa_node();

        if (d1 && d2) {
            auto v = Vector<D*>();
            v.reserve(d1->size + d2->size);

            for (auto d : *d1) v.emplace_back(d);
            for (auto d : *d2) v.emplace_back(d);

            return create(std::move(v));
        }

        if (n1 && n2) {
            if (n1->is_descendant_of(n2)) return n1;
            if (n2->is_descendant_of(n1)) return n2;
            return merge(n1, n2);
        }

        auto n = n1 ? n1 : n2;
        for (auto d : *(d1 ? d1 : d2)) n = insert(n, d);
        return n;
    }

    /// Yields @f$s \setminus \{d\}@f$.
    [[nodiscard]] Set erase(Set s, D* d) {
        if (auto u = s.isa_uniq()) return d == u ? Set() : s;

        if (auto data = s.isa_data()) {
            size_t i = 0, size = data->size;
            for (; i != size; ++i)
                if (data->elems[i] == d) break;

            if (i == size--) return s;
            if (size == 0) return {};
            if (size == 1) return {i == 0 ? data->elems[1] : data->elems[0]};

            assert(size <= N);
            auto [new_data, state] = allocate(size);
            auto db                = data->begin();
            std::copy(db + i + 1, data->end(), std::copy(db, db + i, new_data->elems)); // copy over, skip i

            return unify(new_data, state);
        }

        if (auto n = s.isa_node()) {
            if (d->tid() == 0 || d->tid() < n->min) return n;
            if (!n->contains(d)) return n;

            auto res = erase(n, d);
            if (res->size > N) return res;

            auto v = Vector<D*>();
            v.reserve(res->size);
            for (auto i = res; !i->is_root(); i = i->parent) v.emplace_back(i->def);
            return create(std::move(v));
        }

        return {};
    }
    ///@}

    friend void swap(Sets& s1, Sets& s2) noexcept {
        using std::swap;
        // clang-format off
        swap(s1.data_arena_,  s2.data_arena_);
        swap(s1.node_arena_,  s2.node_arena_);
        swap(s1.pool_,        s2.pool_);
        swap(s1.root_,        s2.root_);
        swap(s1.tid_counter_, s2.tid_counter_);
        swap(s1.id_counter_ , s2.id_counter_ );
        // clang-format on
    }

private:
    D* set_tid(D* def) noexcept {
        assert(def->tid() == 0);
        def->tid_ = tid_counter_++;
        return def;
    }

    // get rid of clang warnings
    template<class T>
    inline static constexpr size_t SizeOf = sizeof(std::conditional_t<std::is_pointer_v<T>, uintptr_t, T>);

    // array helpers
    std::pair<Data*, fe::Arena::State> allocate(size_t size) {
        auto bytes = sizeof(Data) + size * SizeOf<D>;
        auto state = data_arena_.state();
        auto buff  = data_arena_.allocate(bytes, alignof(Data));
        auto data  = new (buff) Data(size);
        return {data, state};
    }

    Set unify(Data* data, fe::Arena::State state, size_t excess = 0) {
        assert(data->size != 0);
        auto [i, ins] = pool_.emplace(data);
        if (ins) {
            data_arena_.deallocate(excess * SizeOf<D>); // release excess memory
            return Set(data);
        }

        data_arena_.deallocate(state);
        return Set(*i);
    }

    // Trie helpers
    constexpr Node* root() const noexcept { return root_.get(); }
    fe::Arena::Ptr<Node> make_node() { return node_arena_.mk<Node>(id_counter_++); }
    fe::Arena::Ptr<Node> make_node(Node* parent, D* def) { return node_arena_.mk<Node>(parent, def, id_counter_++); }

    [[nodiscard]] Node* mount(Node* parent, D* def) {
        assert(def->tid() != 0);
        auto [i, ins] = parent->children.emplace(def, nullptr);
        if (ins) i->second = make_node(parent, def);
        return i->second.get();
    }

    [[nodiscard]] constexpr Node* insert(Node* n, D* d) noexcept {
        if (d->tid() == 0) return mount(n, set_tid(d));
        if (n->def == d) return n;
        if (n->is_root() || n->def->tid() < d->tid()) return mount(n, d);
        return mount(insert(n->parent, d), n->def);
    }

    [[nodiscard]] constexpr Node* merge(Node* n, Node* m) {
        if (n->is_root()) return m;
        if (m->is_root()) return n;
        auto nn = n->def->tid() < m->def->tid() ? n : n->parent;
        auto mm = n->def->tid() > m->def->tid() ? m : m->parent;
        return mount(merge(nn, mm), n->def->tid() < m->def->tid() ? m->def : n->def);
    }

    [[nodiscard]] Node* erase(Node* n, D* d) {
        if (d->tid() > n->def->tid()) return n;
        if (n->def == d) return n->parent;
        return mount(erase(n->parent, d), n->def);
    }

    fe::Arena node_arena_;
    fe::Arena data_arena_;
    absl::flat_hash_set<const Data*, absl::Hash<const Data*>, typename Data::Equal> pool_;
    fe::Arena::Ptr<Node> root_;
    u32 tid_counter_ = 1;
    u32 id_counter_  = 0;
};

} // namespace mim
