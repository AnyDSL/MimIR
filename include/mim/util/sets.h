#pragma once

#include "mim/util/util.h"
#include "mim/util/vector.h"

#include "fe/arena.h"

namespace mim {

template<class D, size_t N = 8> class Sets {
private:
    /// Trie Node.
    class Node {
    public:
        constexpr Node(u32 id) noexcept
            : def(nullptr)
            , parent(nullptr)
            , size_(0)
            , min_(size_t(-1))
            , id_(id) {
            aux.min = u32(-1);
            aux.max = 0;
        }

        constexpr Node(Node* parent, D* def, u32 id) noexcept
            : def(def)
            , parent(parent)
            , size_(parent->size_ + 1)
            , min_(parent->def ? parent->min_ : def->tid())
            , id_(id) {
            parent->link_to_child(this);
        }

        void dot(std::ostream& os) const {
            using namespace std::string_literals;

            auto node2str = [](const Node* n) {
                return "n_" + (n->def ? std::to_string(n->def->tid()) : "root"s) + "_"s + std::to_string(n->id_);
            };

            println(os, "{} [tooltip=\"min: {}, max: {}\"];", node2str(this), aux.min, aux.max);

            for (const auto& [def, child] : children_) println(os, "{} -> {}", node2str(this), node2str(child.get()));

            for (const auto& [_, child] : children_) child->dot(os);

            // clang-format off
            if (auto pa = path_parent()) println(os, "{} -> {} [constraint=false,color=\"#0000ff\",style=dashed];", node2str(this), node2str(pa));
            if (auto up = aux.up      ) println(os, "{} -> {} [constraint=false,color=\"#ff0000\"];", node2str(this), node2str(up));
            if (auto dn = aux.down    ) println(os, "{} -> {} [constraint=false,color=\"#00ff00\"];", node2str(this), node2str(dn));
            // clang-format on
        }

        ///@name Getters
        ///@{
        constexpr bool empty() const noexcept { return def == nullptr; }
        constexpr bool is_root() const noexcept { return empty(); }
        ///@}

        ///@name parent
        ///@{
        // clang-format off
        constexpr Node*  aux_parent() noexcept { return aux.parent && (aux.parent->aux.down == this || aux.parent->aux.up == this) ? aux.parent : nullptr; }
        constexpr Node* path_parent() noexcept { return aux.parent && (aux.parent->aux.down != this && aux.parent->aux.up != this) ? aux.parent : nullptr; }
        // clang-format on
        ///@}

        ///@name Aux Tree (Splay Tree)
        ///@{

        /// [Splays](https://hackmd.io/@CharlieChuang/By-UlEPFS#Operation1) `this` to the root of its splay tree.
        constexpr void splay() noexcept {
            while (auto p = aux_parent()) {
                if (auto pp = p->aux_parent()) {
                    if (p->aux.down == this && pp->aux.down == p) { // zig-zig
                        pp->ror();
                        p->ror();
                    } else if (p->aux.up == this && pp->aux.up == p) { // zag-zag
                        pp->rol();
                        p->rol();
                    } else if (p->aux.down == this && pp->aux.up == p) { // zig-zag
                        p->ror();
                        pp->rol();
                    } else { // zag-zig
                        assert(p->aux.up == this && pp->aux.down == p);
                        p->rol();
                        pp->ror();
                    }
                } else if (p->aux.down == this) { // zig
                    p->ror();
                } else { // zag
                    assert(p->aux.up == this);
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
            constexpr auto child = [](Node* n, size_t i) -> Node*& { return i == 0 ? n->aux.down : n->aux.up; };

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

            x->update();
            c->update();
        }

        constexpr void rol() noexcept { return rotate<0>(); }
        constexpr void ror() noexcept { return rotate<1>(); }

        constexpr void update() noexcept {
            if (is_root()) {
                aux.min = u32(-1);
                aux.max = 0;
            } else {
                aux.min = aux.max = def->tid();
            }

            if (auto d = aux.down) {
                aux.min = std::min(aux.min, d->aux.min);
                aux.max = std::max(aux.max, d->aux.max);
            }
            if (auto u = aux.up) {
                aux.min = std::min(aux.min, u->aux.min);
                aux.max = std::max(aux.max, u->aux.max);
            }
        }

        [[nodiscard]] constexpr std::pair<u32, u32> aggregate() noexcept {
            expose();
            return {aux.min, aux.max};
        }
        ///@}

        /// @name Link-Cut-Tree
        /// This is a simplified version of a Link-Cut-Tree without the Cut operation.
        ///@{

        /// Registers the edge `this -> child` in the *aux* tree.
        constexpr void link_to_child(Node* child) noexcept {
            this->expose();
            child->expose();
            if (!child->aux.up) {
                this->aux.parent = child;
                child->aux.up    = this;
                child->update();
            }
        }

        /// Make a preferred path from `this` to root while putting `this` at the root of the *aux* tree.
        /// @returns the last valid path_parent().
        constexpr Node* expose() noexcept {
            Node* prev = nullptr;
            for (auto curr = this; curr; prev = curr, curr = curr->aux.parent) {
                curr->splay();
                assert(!prev || prev->aux.parent == curr);
                curr->aux.down = prev;
                curr->update();
            }
            splay();
            return prev;
        }

        /// Least Common Ancestor of `this` and @p other in the *rep* tree.
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

        // TODO remove?
        constexpr size_t min() const noexcept { return min_; }
        constexpr size_t max() const noexcept { return def->tid(); }
        constexpr size_t within(D* d) const noexcept { return min() <= d->tid() && d->tid() <= max(); }

        D* def;
        Node* parent;
        size_t size_;
        size_t min_;
        GIDMap<D*, fe::Arena::Ptr<Node>> children_;
        u32 id_;

        struct {
            Node* parent = nullptr; ///< parent or path-parent
            Node* down   = nullptr; ///< left/deeper/down/leaf-direction
            Node* up     = nullptr; ///< right/shallower/up/root-direction
            u32 min      = u32(-1);
            u32 max      = 0;
        } mutable aux;
    };

public:
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
        constexpr D* const* begin() const noexcept { return elems; }
        constexpr D* const* end() const noexcept { return elems + size; }
        ///@}

        template<class H> friend constexpr H AbslHashValue(H h, const Data* d) noexcept {
            if (!d) return H::combine(std::move(h), 0);
            return H::combine_contiguous(std::move(h), d->elems, d->size);
        }
    };

    enum Tag : uintptr_t {
        Empty  = 0,
        Single = 1,
        Array  = 2,
        Trie   = 3,
    };

    class Set {
    public:
        class iterator {
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
            constexpr iterator(D* d) noexcept
                : tag_(Tag::Single)
                , def_(d) {}
            constexpr iterator(D* const* elems) noexcept
                : tag_(Tag::Array)
                , elems_(elems) {}
            constexpr iterator(Node* node) noexcept
                : tag_(Tag::Trie)
                , node_(node) {}
            ///@}

            /// @name Increment
            /// @note These operations only change the *view* of this Set at the Trie; the Trie itself is **not**
            /// modified.
            ///@{
            constexpr iterator& operator++() noexcept {
                // clang-format off
                switch (tag_) {
                    case Tag::Single: return clear();
                    case Tag::Array:  return ++elems_, *this;
                    case Tag::Trie: {
                        node_      = node_->parent;
                        if (node_->is_root()) clear();
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
                    case Tag::Single: return def_;
                    case Tag::Array: return *elems_;
                    case Tag::Trie: return node_->def;
                    default: fe::unreachable();
                }
            }
            constexpr pointer operator->() const noexcept { return this->operator*(); }
            ///@}

            iterator& clear() { return tag_ = Tag::Empty, ptr_ = 0, *this; }

        private:
            Tag tag_;
            union {
                D* def_;
                D* const* elems_;
                Node* node_;
                uintptr_t ptr_;
            };
        };

        /// @name Construction
        ///@{
        constexpr Set(const Set&) noexcept = default;
        constexpr Set(Set&&) noexcept      = default;
        constexpr Set() noexcept           = default; ///< Empty set
        constexpr Set(D* d) noexcept                  ///< Singleton set.
            : ptr_(uintptr_t(d) | uintptr_t(Tag::Single)) {}
        constexpr Set(const Data* data) noexcept ///< Array Set.
            : ptr_(uintptr_t(data) | uintptr_t(Tag::Array)) {}
        constexpr Set(Node* node) noexcept ///< Trie set.
            : ptr_(uintptr_t(node) | uintptr_t(Tag::Trie)) {}

        constexpr Set& operator=(const Set&) noexcept = default;
        ///@}

        /// @name Getters
        ///@{
        constexpr Tag tag() const noexcept { return Tag(ptr_ & uintptr_t(0b11)); }
        template<class T = uintptr_t> constexpr T* ptr() const noexcept {
            return reinterpret_cast<T*>(ptr_ & (uintptr_t(-1) << uintptr_t(2)));
        }
        constexpr bool empty() const noexcept { return ptr_ == 0; } ///< Not empty.
        constexpr explicit operator bool() const noexcept { return ptr_ != 0; }

        constexpr size_t size() const noexcept {
            // clang-format off
            switch (tag()) {
                case Tag::Empty:  return 0;
                case Tag::Single: return 1;
                case Tag::Array:  return ptr<Data>()->size;
                case Tag::Trie:   return ptr<Node>()->size_;
                default: fe::unreachable();
            }
            // clang-format on
        }

        ///@}

        /// @name Check Membership
        ///@{
        bool contains(D* d) const noexcept {
            switch (tag()) {
                case Tag::Empty: return false;
                case Tag::Single: return ptr<D>() == d;

                case Tag::Array:
                    for (auto e : *ptr<Data>())
                        if (d == e) return true;
                    return false;

                case Tag::Trie: {
                    auto cur = ptr<Node>();
                    if (!cur->within(d)) return false;

                    auto tid = d->tid();
                    if (auto [min, max] = cur->aggregate(); tid < min || tid > max) return false;

                    while (cur) {
                        if (cur->def == d) return true;
                        cur = (!cur->def || tid > cur->def->tid()) ? cur->aux.down : cur->aux.up;
                    }

                    return false;
                }
                default: fe::unreachable();
            }
        }

        [[nodiscard]] bool has_intersection(Set other) const noexcept {
            if (this->empty() || other.empty()) return false;
            if (this->tag() == Tag::Single) return other.contains(this->template ptr<D>());
            if (other.tag() == Tag::Single) return this->contains(other.template ptr<D>());

            if (other.tag() == Tag::Array && other.tag() == Tag::Array) {
                auto d1 = this->template ptr<Data>();
                auto d2 = other.template ptr<Data>();
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

            if (this->tag() == Tag::Trie && other.tag() == Tag::Trie) {
                auto n = this->ptr<Node>();
                auto m = other.ptr<Node>();

                if (!n->lca(m)->is_root()) return true;

                while (!n->is_root() && !m->is_root()) {
                    if (n->def == m->def) return true;
                    if (n->min() > other.template ptr<Node>()->max() || n->max() < m->min())
                        return false; // TODO double-check

                    if (n->def->tid() > m->def->tid())
                        n = n->parent;
                    else
                        m = m->parent;
                }

                return false;
            }

            assert((this->tag() == Tag::Trie && other.tag() == Tag::Array)
                   || (this->tag() == Tag::Array && other.tag() == Tag::Trie));

            auto data = this->tag() == Tag::Array ? this->template ptr<Data>() : other.template ptr<Data>();
            for (auto e : *data)
                if (contains(e)) return true;
            return false;
        }
        ///@}

        /// @name Iterators
        ///@{
        constexpr iterator begin() const noexcept {
            // clang-format off
            switch (tag()) {
                case Tag::Empty:  return iterator();
                case Tag::Single: return iterator(ptr<D>());
                case Tag::Array:  return iterator(ptr<Data>()->begin());
                case Tag::Trie:   return iterator(ptr<Node>());
                default: fe::unreachable();
            }
            // clang-format on
        }

        constexpr iterator end() const noexcept {
            switch (tag()) {
                case Tag::Empty:
                case Tag::Single:
                case Tag::Trie: return iterator();
                case Tag::Array: return iterator(ptr<Data>()->end());
                default: fe::unreachable();
            }
        }
        ///@}

        /// @name Comparisons
        ///@{
        constexpr bool operator==(Set other) const noexcept { return this->ptr_ == other.ptr_; }
        constexpr bool operator!=(Set other) const noexcept { return this->ptr_ != other.ptr_; }
        ///@}

        void dump() const {
            std::cout << size() << " - {";
            auto sep = "";
            for (auto d : *this) {
                std::cout << sep << d->gid() << '/' << d->tid();
                sep = ", ";
            }
            std::cout << '}' << std::endl;
        }

    private:
        uintptr_t ptr_ = 0;
    };

    static_assert(std::forward_iterator<typename Set::iterator>);
    static_assert(std::ranges::range<Set>);

    /// @name Construction
    ///@{
    Sets& operator=(const Sets&) = delete;

    constexpr Sets() noexcept            = default;
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
        auto db = v.begin();
        auto de = v.end();
        std::sort(db, de, [](D* d1, D* d2) { return d1->gid() < d2->gid(); });
        auto di   = std::unique(db, de);
        auto size = std::distance(db, di);

        if (size == 0) return {};
        if (size == 1) return {*db};

        if (size_t(size) <= N) {
            auto [data, state] = allocate(size);
            std::copy(db, di, data->elems);
            return unify(data, state);
        }

        // Sort in ascending tids but 0 goes last.
        std::sort(db, de, [](D* d1, D* d2) { return d1->tid() != 0 && (d2->tid() == 0 || d1->tid() < d2->tid()); });

        auto res = root();
        for (auto i = db, e = di; i != e; ++i) res = insert(res, *i);
        return res;
    }

    /// Yields @f$set \cup \{d\}@f$.
    [[nodiscard]] Set insert(Set set, D* d) {
        switch (set.tag()) {
            case Tag::Empty: return {d};

            case Tag::Single: {
                auto e = set.template ptr<D>();
                if (d == e) return {d};

                auto [data, state] = allocate(2);
                if (d->gid() < e->gid())
                    data->elems[0] = d, data->elems[1] = e;
                else
                    data->elems[0] = e, data->elems[1] = d;
                return unify(data, state);
            }

            case Tag::Array: { // TODO more subcases
                auto data = set.template ptr<const Data>();
                auto v    = Vector<D*>(data->begin(), data->end());
                v.emplace_back(d);
                return create(v);
            }

            case Tag::Trie: return insert(set.template ptr<Node>(), d);

            default: fe::unreachable();
        }
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a.empty() || a == b) return b;
        if (b.empty()) return a;

        if (a.tag() == Tag::Single) return insert(b, a.template ptr<D>());
        if (b.tag() == Tag::Single) return insert(a, b.template ptr<D>());

        if (a.tag() == Tag::Trie && b.tag() == Tag::Trie) return merge(a.template ptr<Node>(), b.template ptr<Node>());

        if (a.tag() == Tag::Array && b.tag() == Tag::Array) {
            auto v  = Vector<D*>();
            auto da = a.template ptr<Data>();
            auto db = b.template ptr<Data>();
            v.reserve(da->size + db->size);

            for (auto i : *da) v.emplace_back(i);
            for (auto i : *db) v.emplace_back(i);

            return create(v);
        }

        assert((a.tag() == Tag::Trie && b.tag() == Tag::Array) || (a.tag() == Tag::Array && b.tag() == Tag::Trie));
        auto res  = root();
        auto data = a.tag() == Tag::Array ? a.template ptr<Data>() : b.template ptr<Data>();
        for (auto d : *data) res = insert(res, d);
        return res;
    }

    /// Yields @f$set \setminus def@f$.
    [[nodiscard]] Set erase(Set set, D* d) {
        if (set.empty()) return {};

        if (set.tag() == Tag::Single) {
            auto e = set.template ptr<D>();
            return d == e ? set : Set();
        }

        if (set.tag() == Tag::Array) {
            size_t x  = size_t(-1);
            auto data = set.template ptr<Data>();
            for (size_t i = 0, e = data->size; i != e; ++i) {
                if (data->elems[i] == d) {
                    x = i;
                    break;
                }
            }

            if (x == size_t(-1)) return set;

            auto size = data->size - 1;
            if (size == 0) return {};
            if (size == 1) return {x == 0 ? data->elems[1] : data->elems[0]};

            auto [new_data, state] = allocate(size);
            std::copy(data->begin() + x + 1, data->end(),
                      std::copy(data->begin(), data->begin() + x, new_data->elems)); // copy over, skip i
            return unify(new_data, state);
        }

        assert(set.tag() == Tag::Trie);
        return erase(set.template ptr<Node>(), d);
    }
    ///@}

    friend void swap(Sets& s1, Sets& s2) noexcept {
        using std::swap;
        // clang-format off
        swap(s1.array_arena_, s2.array_arena_);
        swap(s1. node_arena_, s2. node_arena_);
        swap(s1.pool_,        s2.pool_);
        swap(s1.root_,        s2.root_);
        swap(s1.tid_counter_, s2.tid_counter_);
        swap(s1.id_counter_ , s2.id_counter_ );
        // clang-format on
    }

    D* set_tid(D* def) noexcept {
        assert(def->tid() == 0);
        def->tid_ = tid_counter_++;
        return def;
    }

private:
    // get rid of clang warnings
    template<class T>
    inline static constexpr size_t SizeOf = sizeof(std::conditional_t<std::is_pointer_v<T>, uintptr_t, T>);

    // array helpers
    std::pair<Data*, fe::Arena::State> allocate(size_t size) {
        assert(0 < size && size <= N);
        auto bytes = sizeof(Data) + size * SizeOf<D>;
        auto state = array_arena_.state();
        auto buff  = array_arena_.allocate(bytes, alignof(Data));
        auto data  = new (buff) Data(size);
        return {data, state};
    }

    Set unify(Data* data, fe::Arena::State state, size_t excess = 0) {
        assert(data->size != 0);
        auto [i, ins] = pool_.emplace(data);
        if (ins) {
            array_arena_.deallocate(excess * SizeOf<D>); // release excess memory
            return Set(data);
        }

        array_arena_.deallocate(state);
        return Set(*i);
    }

    constexpr static void copy_if_unique_and_inc(D**& i, D* const*& ai) noexcept {
        if (*i != *ai) *++i = *ai;
        ++ai;
    }

    // Trie helpers
    constexpr Node* root() const noexcept { return root_.get(); }
    fe::Arena::Ptr<Node> make_node() { return node_arena_.mk<Node>(id_counter_++); }
    fe::Arena::Ptr<Node> make_node(Node* parent, D* def) { return node_arena_.mk<Node>(parent, def, id_counter_++); }

    Node* mount(Node* parent, D* def) {
        assert(def->tid() != 0);
        auto [i, ins] = parent->children_.emplace(def, nullptr);
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
        if (n->is_descendant_of(m)) return n;
        if (m->is_descendant_of(n)) return m;

        auto nn = n->def->tid() < m->def->tid() ? n : n->parent;
        auto mm = n->def->tid() > m->def->tid() ? m : m->parent;
        return mount(merge(nn, mm), n->def->tid() < m->def->tid() ? m->def : n->def);
    }

    [[nodiscard]] Node* erase(Node* n, D* d) {
        if (d->tid() == 0 || !n->within(d)) return {n};
        if (n->def == d) return n->parent;
        return mount(erase(n->parent, d), n->def);
    }

    fe::Arena node_arena_;
    fe::Arena array_arena_;
    absl::flat_hash_set<const Data*, absl::Hash<const Data*>, typename Data::Equal> pool_;
    fe::Arena::Ptr<Node> root_;
    u32 tid_counter_ = 1;
    u32 id_counter_  = 0;
};

} // namespace mim
