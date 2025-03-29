#pragma once

#include <algorithm>
#include <iterator>

#include <absl/container/flat_hash_set.h>
#include <fe/arena.h>

#include "mim/util/util.h"

namespace mim {

// get rid of clang warnings
template<class T>
inline static constexpr size_t SizeOf = sizeof(std::conditional_t<std::is_pointer_v<T>, uintptr_t, T>);

/// Maintains PooledSet%s within a `fe::Arena` and unifies them in a `absl::flat_hash_set`.
template<class D> class Pool_ {
public:
    /// Ordered set maintained in a consecutive buffer and unified in Pool.
    class Set {
    public:
        struct Data {
            constexpr Data() noexcept = default;
            constexpr Data(size_t size) noexcept
                : size(size) {}

            size_t size = 0;
            D* elems[];

            struct Equal {
                constexpr bool operator()(const Data* d1, const Data* d2) const {
                    bool res = d1->size == d2->size;
                    for (size_t i = 0, e = d1->size; res && i != e; ++i) res &= d1->elems[i] == d2->elems[i];
                    return res;
                }
            };

            template<class H> friend constexpr H AbslHashValue(H h, const Data* d) {
                if (!d) return H::combine(std::move(h), 0);
                return H::combine_contiguous(std::move(h), d->elems, d->size);
            }
        };

        static_assert(sizeof(Data) == sizeof(size_t), "Data.elems should be 0");

    private:
        /// @name Construction & Destruction
        ///@{
        constexpr Set(const Data* data) noexcept
            : data_(data) {}

    public:
        constexpr Set() noexcept                      = default;
        constexpr Set(const Set&) noexcept            = default;
        constexpr Set& operator=(const Set&) noexcept = default;
        constexpr void clear() noexcept { data_ = nullptr; }
        ///@}

        /// @name Getters
        ///@{
        constexpr explicit operator bool() const noexcept { return data_ != nullptr; } ///< Is not empty?
        constexpr bool empty() const noexcept { return data_ == nullptr; }
        constexpr size_t size() const noexcept { return empty() ? 0 : data_->size; }
        constexpr const D*& operator[](size_t i) const noexcept { return data_->elems[i]; }
        constexpr const D*& front() const noexcept { return (*this)[0]; }
        constexpr const D* const* elems() const noexcept { return data_ ? data_->elems : nullptr; }
        [[nodiscard]] constexpr bool contains(D* elem) const noexcept {
            return binary_find(begin(), end(), elem, [](D* d1, D* d2) { return d1->gid() < d2->gid(); }) != end();
        }

        /// Is @f$this \cup other \neq \emptyset@f$?
        [[nodiscard]] constexpr bool has_intersection(Set other) const noexcept {
            if (*this == other) return true;
            if (!*this || !other) return false;

            for (auto ai = this->begin(), ae = this->end(), bi = other.begin(), be = other.end();
                 ai != ae && bi != be;) {
                if (*ai == *bi) return true;

                if ((*ai)->gid() < (*bi)->gid())
                    ++ai;
                else
                    ++bi;
            }

            return false;
        }
        ///@}

        /// @name Comparisons
        ///@{
        constexpr bool operator==(Set other) const noexcept { return this->data_ == other.data_; }
        constexpr bool operator!=(Set other) const noexcept { return this->data_ != other.data_; }
        ///@}

        /// @name Iterators
        ///@{
        constexpr auto begin() const noexcept { return elems(); }
        constexpr auto end() const noexcept { return elems() + size(); } // note: you can add 0 to nullptr
        constexpr auto cbegin() const noexcept { return elems(); }
        constexpr auto cend() const noexcept { return end(); }
        constexpr auto rbegin() const noexcept { return std::reverse_iterator(end()); }
        constexpr auto rend() const noexcept { return std::reverse_iterator(begin()); }
        constexpr auto crbegin() const noexcept { return rbegin(); }
        constexpr auto crend() const noexcept { return rend(); }
        ///@}

    private:
        const Data* data_ = nullptr;

        template<class H> friend constexpr H AbslHashValue(H h, Set set) noexcept {
            return H::combine(std::move(h), set.data_);
        }
        friend class Pool_<D>;
    };

    using Data = typename Set::Data;

    /// @name Construction
    ///@{
    Pool_& operator=(const Pool_&) = delete;

    constexpr Pool_() noexcept             = default;
    constexpr Pool_(const Pool_&) noexcept = delete;
    constexpr Pool_(Pool_&& other) noexcept
        : Pool_() {
        swap(*this, other);
    }
    ///@}

    /// @name Set Operations
    /// @note These operations do **not** modify the input set(s); they create a **new** PooledSet.
    ///@{

    /// Create a PooledSet wih a *single* @p elem%ent: @f$\{elem\}@f$.
    [[nodiscard]] Set create(D* def) {
        auto [data, state] = allocate(1);
        data->elems[0]     = def;
        return unify(data, state);
    }

    /// Create a PooledSet wih all elements in the given range.
    template<class I> [[nodiscard]] Set create(I begin, I end) {
        auto size = std::distance(begin, end); // max space needed - may be less; see actual_size below
        if (size == 0) return {};

        auto [data, state] = allocate(size);
        auto db = data->elems, de = data->elems + size;

        std::copy(begin, end, db);
        std::sort(db, de, GIDLt<D>());
        auto di          = std::unique(db, de);
        auto actual_size = std::distance(db, di);
        data->size       = actual_size; // correct size
        return unify(data, state, size - actual_size);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] Set merge(Set a, Set b) {
        if (a == b || !b) return a;
        if (!a) return b;

        auto size          = a.size() + b.size(); // max space needed - may be less; see actual_size below
        auto [data, state] = allocate(size);

        auto di = data->elems;
        auto ai = a.begin(), ae = a.end(), bi = b.begin(), be = b.end();

        *di = (*ai)->gid() < (*bi)->gid() ? *ai++ : *bi++; // di points to the last valid elem, so init first one

        while (ai != ae && bi != be)
            if ((*ai)->gid() < (*bi)->gid())
                copy_if_unique_and_inc(di, ai);
            else
                copy_if_unique_and_inc(di, bi);

        // copy the rest of a/b (if needed):
        while (ai != ae) copy_if_unique_and_inc(di, ai);
        while (bi != be) copy_if_unique_and_inc(di, bi);

        auto actual_size = std::distance(data->elems, di + 1);
        data->size       = actual_size; // correct size
        return unify(data, state, size - actual_size);
    }

    /// Yields @f$a \cap b@f$.
    /// @todo Not yet tested.
    [[nodiscard]] Set intersect(Set a, Set b) {
        if (a == b) return a;
        if (!a || !b) return {};

        auto size          = std::min(a.size(), b.size()); // max space needed - may be less; see actual_size below
        auto [data, state] = allocate(size);

        auto di = data->elems;
        for (auto ai = a.begin(), ae = a.end(), bi = b.begin(), be = b.end(); ai != ae && bi != be;)
            if (*ai == *bi)
                *di++ = ++ai, *bi++;
            else if ((*ai)->gid() < (*bi)->gid())
                ++ai;
            else
                ++bi;

        auto actual_size = std::distance(data->elems, di);
        data->size       = actual_size; // correct size
        return unify(data, state, size - actual_size);
    }

    /// Yields @f$set \cup \{elem\}@f$.
    [[nodiscard]] Set insert(Set set, D* def) { return merge(set, create(def)); }

    /// Yields @f$set \setminus elem@f$.
    [[nodiscard]] Set erase(Set set, D* def) {
        if (!set) return set;
        auto i = binary_find(set.begin(), set.end(), def, [](D* d1, D* d2) { return d1->gid() < d2->gid(); });
        if (i == set.end()) return set;

        auto size = set.size() - 1;
        if (size == 0) return {}; // empty Set is not hashed

        auto [data, state] = allocate(size);
        std::copy(i + 1, set.end(), std::copy(set.begin(), i, data->elems)); // copy over, skip i
        return unify(data, state);
    }
    ///@}

    friend void swap(Pool_& p1, Pool_& p2) noexcept {
        using std::swap;
        swap(p1.arena_, p2.arena_);
        swap(p1.pool_, p2.pool_);
    }

private:
    std::pair<Data*, fe::Arena::State> allocate(size_t size) {
        auto bytes = sizeof(Data) + size * SizeOf<D>;
        auto state = arena_.state();
        auto buff  = arena_.allocate(bytes, alignof(Data));
        auto data  = new (buff) Data(size);
        return {data, state};
    }

    Set unify(Data* data, fe::Arena::State state, size_t excess = 0) {
        if (data->size == 0) {
            arena_.deallocate(state);
            return {};
        }

        auto [i, ins] = pool_.emplace(data);
        if (ins) {
            arena_.deallocate(excess * SizeOf<D>); // release excess memory
            return Set(data);
        }

        arena_.deallocate(state);
        return Set(*i);
    }

    void copy_if_unique_and_inc(D**& i, D* const*& ai) {
        if (*i != *ai) *++i = *ai;
        ++ai;
    }

    fe::Arena arena_;
    absl::flat_hash_set<const Data*, absl::Hash<const Data*>, typename Data::Equal> pool_;
};

template<typename D> using PoolSet = typename mim::Pool_<D>::Set;

} // namespace mim
