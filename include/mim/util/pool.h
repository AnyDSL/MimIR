#pragma once

#include <algorithm>
#include <iterator>

#include <absl/container/flat_hash_set.h>
#include <fe/arena.h>

#include "mim/util/util.h"

// TODO move to fe

namespace mim {

// get rid of clang warnings
template<class T>
inline static constexpr size_t SizeOf = sizeof(std::conditional_t<std::is_pointer_v<T>, uintptr_t, T>);

template<class T> class Pool;

/// Ordered set maintained in a consecutive buffer and unified in Pool.
template<class T> class PooledSet {
public:
    struct Data {
        constexpr Data() noexcept = default;
        constexpr Data(size_t size) noexcept
            : size(size) {}

        size_t size = 0;
        T elems[];

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
    constexpr PooledSet(const Data* data) noexcept
        : data_(data) {}

public:
    constexpr PooledSet() noexcept                            = default;
    constexpr PooledSet(const PooledSet&) noexcept            = default;
    constexpr PooledSet& operator=(const PooledSet&) noexcept = default;
    constexpr void clear() noexcept { data_ = nullptr; }
    ///@}

    /// @name Getters
    ///@{
    constexpr explicit operator bool() const noexcept { return data_ != nullptr; } ///< Is not empty?
    constexpr bool empty() const noexcept { return data_ == nullptr; }
    constexpr size_t size() const noexcept { return empty() ? 0 : data_->size; }
    constexpr const T& operator[](size_t i) const { return data_->elems[i]; }
    constexpr const T& front() const { return (*this)[0]; }
    constexpr const T* elems() const { return data_ ? data_->elems : nullptr; }
    constexpr bool contains(const T& elem) const { return binary_find(begin(), end(), elem, GIDLt<T>()) != end(); }

    /// Is @f$this \cup other \neq \emptyset@f$?
    [[nodiscard]] bool has_intersection(PooledSet<T> other) {
        if (*this == other) return true;
        if (!*this || !other) return false;

        for (auto ai = this->begin(), ae = this->end(), bi = other.begin(), be = other.end(); ai != ae && bi != be;) {
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
    constexpr bool operator==(PooledSet<T> other) const noexcept { return this->data_ == other.data_; }
    constexpr bool operator!=(PooledSet<T> other) const noexcept { return this->data_ != other.data_; }
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

    template<class H> friend H AbslHashValue(H h, PooledSet<T> set) { return H::combine(std::move(h), set.data_); }
    friend class Pool<T>;
};

#ifndef DOXYGEN
} // namespace mim

template<class T> struct std::hash<mim::PooledSet<T>> {
    constexpr size_t operator()(mim::PooledSet<T> set) const { return std::hash<uintptr_t>()((uintptr_t)set.data_); }
};

namespace mim {
#endif

/// Maintains PooledSet%s within a `fe::Arena` and unifies them in a `absl::flat_hash_set`.
template<class T> class Pool {
    using Data = typename PooledSet<T>::Data;

public:
    /// @name Construction & Destruction
    ///@{
    Pool& operator=(const Pool&) = delete;

    Pool()            = default;
    Pool(const Pool&) = delete;
    Pool(Pool&& other)
        : Pool() {
        swap(*this, other);
    }
    ///@}

    /// @name Set Operations
    /// @note These operations do **not** modify the input set(s); they create a **new** PooledSet.
    ///@{
    /// Create a PooledSet wih a *single* @p elem%ent: @f$\{elem\}@f$.
    [[nodiscard]] PooledSet<T> create(T elem) {
        auto [data, state] = allocate(1);
        data->elems[0]     = elem;
        return unify(data, state);
    }

    /// Create a PooledSet wih all elements in the given range.
    template<class I> [[nodiscard]] PooledSet<T> create(I begin, I end) {
        auto size = std::distance(begin, end); // max space needed - may be less; see actual_size below
        if (size == 0) return {};

        auto [data, state] = allocate(size);
        auto db = data->elems, de = data->elems + size;

        std::copy(begin, end, db);
        std::sort(db, de, GIDLt<T>());
        auto di          = std::unique(db, de);
        auto actual_size = std::distance(db, di);
        data->size       = actual_size; // correct size
        return unify(data, state, size - actual_size);
    }

    /// Yields @f$a \cup b@f$.
    [[nodiscard]] PooledSet<T> merge(PooledSet<T> a, PooledSet<T> b) {
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
    [[nodiscard]] PooledSet<T> intersect(PooledSet<T> a, PooledSet<T> b) {
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

    /// Yields @f$a \cup \{elem\}@f$.
    [[nodiscard]] PooledSet<T> insert(PooledSet<T> a, const T& elem) { return merge(a, create(elem)); }

    /// Yields @f$a \setminus elem@f$.
    [[nodiscard]] PooledSet<T> erase(PooledSet<T> a, const T& elem) {
        if (!a) return a;
        auto i = binary_find(a.begin(), a.end(), elem, GIDLt<T>());
        if (i == a.end()) return a;

        auto size = a.size() - 1;
        if (size == 0) return PooledSet<T>(); // empty Set is not hashed

        auto [data, state] = allocate(size);
        std::copy(i + 1, a.end(), std::copy(a.begin(), i, data->elems)); // copy over, skip i
        return unify(data, state);
    }
    ///@}

    friend void swap(Pool& p1, Pool& p2) noexcept {
        using std::swap;
        swap(p1.arena_, p2.arena_);
        swap(p1.pool_, p2.pool_);
    }

private:
    std::pair<Data*, fe::Arena::State> allocate(size_t size) {
        auto bytes = sizeof(Data) + size * SizeOf<T>;
        auto state = arena_.state();
        auto buff  = arena_.allocate(bytes);
        auto data  = new (buff) Data(size);
        return {data, state};
    }

    PooledSet<T> unify(Data* data, fe::Arena::State state, size_t excess = 0) {
        if (data->size == 0) {
            arena_.deallocate(state);
            return {};
        }

        auto [i, ins] = pool_.emplace(data);
        if (ins) {
            arena_.deallocate(excess * SizeOf<T>); // release excess memory
            return PooledSet<T>(data);
        }

        arena_.deallocate(state);
        return PooledSet<T>(*i);
    }

    void copy_if_unique_and_inc(T*& i, const T*& ai) {
        if (*i != *ai) *++i = *ai;
        ++ai;
    }

    fe::Arena arena_;
    absl::flat_hash_set<const Data*, absl::Hash<const Data*>, typename Data::Equal> pool_;
};

} // namespace mim
