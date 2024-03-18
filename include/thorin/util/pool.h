#pragma once

#include <absl/container/flat_hash_set.h>
#include <fe/arena.h>

#include "thorin/util/util.h"

// TODO move to fe

namespace thorin {

// get rid of clang warnings
template<class T>
inline static constexpr size_t SizeOf = sizeof(std::conditional_t<std::is_pointer_v<T>, uintptr_t, T>);

template<class T> class Pool;

template<class T> class PooledSet {
public:
    struct Data {
        Data() noexcept = default;
        Data(size_t size) noexcept
            : size(size) {}

        size_t size = 0;
        T elems[];

        struct Equal {
            bool operator()(const Data* d1, const Data* d2) const {
                bool res = d1->size == d2->size;
                for (size_t i = 0, e = d1->size; res && i != e; ++i) res &= d1->elems[i] == d2->elems[i];
                return res;
            }
        };

        template<class H> friend H AbslHashValue(H h, const Data* d) {
            if (!d) return H::combine(std::move(h), 0);
            return H::combine_contiguous(std::move(h), d->elems, d->size);
        }
    };

    static_assert(sizeof(Data) == sizeof(size_t), "Data.elems should be 0");

private:
    PooledSet(const Data* data) noexcept
        : data_(data) {}

public:
    PooledSet() noexcept = default;

    /// @name Getters
    ///@{
    explicit operator bool() const { return data_; } ///< Is not empty?
    bool empty() const { return data_ == nullptr; }
    size_t size() const { return empty() ? 0 : data_->size; }
    const T& operator[](size_t i) const { return data_->elems[i]; }
    const T& front() const { return (*this)[0]; }
    const T* elems() const { return data_ ? data_->elems : nullptr; }
    bool contains(const T& elem) const {
        auto res = binary_find(begin(), end(), elem, GIDLt<T>()) != end();
        // outln("contains({, } - {}): {}", *this, elem, res);
        return res;
    }
    ///@}

    /// @name Comparisons
    ///@{
    bool operator==(PooledSet<T> other) const { return this->data_ == other.data_; }
    bool operator!=(PooledSet<T> other) const { return this->data_ != other.data_; }
    ///@}

    /// @name Iterators
    ///@{
    auto begin() const { return elems(); }
    auto end() const { return elems() + size(); } // note: you can add 0 to nullptr
    auto cbegin() const { return elems(); }
    auto cend() const { return end(); }
    auto rbegin() const { return std::reverse_iterator(end()); }
    auto rend() const { return std::reverse_iterator(begin()); }
    auto crbegin() const { return rbegin(); }
    auto crend() const { return rend(); }
    ///@}

private:
    const Data* data_ = nullptr;

    template<class H> friend H AbslHashValue(H h, PooledSet<T> set) { return H::combine(std::move(h), set.data_); }
    friend class Pool<T>;
};

#ifndef DOXYGEN
} // namespace thorin

template<class T> struct std::hash<thorin::PooledSet<T>> {
    size_t operator()(thorin::PooledSet<T> set) const { return std::hash<uintptr_t>()((uintptr_t)set.data_); }
};

namespace thorin {
#endif

template<class T> class Pool {
public:
    Pool()            = default;
    Pool(const Pool&) = delete;
    Pool(Pool&& other)
        : Pool() {
        swap(*this, other);
    }
    Pool& operator=(const Pool&) = delete;

    [[nodiscard]] PooledSet<T> singleton(T elem) {
        static constexpr auto size = sizeof(typename PooledSet<T>::Data) + SizeOf<T>;

        auto state     = arena_.state();
        auto buf       = arena_.allocate(size);
        auto data      = new (buf) typename PooledSet<T>::Data(1);
        data->elems[0] = elem;

        auto [i, ins] = pool_.emplace(data);
        if (ins) return PooledSet<T>(data);
        arena_.deallocate(state);
        return PooledSet<T>(*i);
    }

    [[nodiscard]] PooledSet<T> merge(PooledSet<T> a, PooledSet<T> b) {
        if (a == b || !b) return a;
        if (!a) return b;

        // outln("--\nmerge({, } - {, })", a, b);

        auto size  = a.size() + b.size(); // max space needed - may be less; see actual_size below
        auto bytes = sizeof(typename PooledSet<T>::Data) + size * SizeOf<T>;
        auto state = arena_.state();
        auto buf   = arena_.allocate(bytes);
        auto data  = new (buf) typename PooledSet<T>::Data(size);

        auto di = data->elems;
        auto ai = a.begin(), ae = a.end(), bi = b.begin(), be = b.end();

        *di = (*ai)->gid() < (*bi)->gid() ? *ai++ : *bi++;

        while (ai != ae && bi != be)
            if ((*ai)->gid() < (*bi)->gid())
                copy_and_inc_unique(di, ai);
            else
                copy_and_inc_unique(di, bi);

        // copy the rest of a/b (if needed):
        while (ai != ae) copy_and_inc_unique(di, ai);
        while (bi != be) copy_and_inc_unique(di, bi);

        auto actual_size = std::distance(data->elems, di + 1);
        data->size       = actual_size; // correct size

        // outln("d: {, }", PooledSet<T>(data));
        auto [i, ins] = pool_.emplace(data);
        if (ins) {
            arena_.deallocate(size - actual_size); // release excess memory
            // outln("fresh: {, }", PooledSet<T>(data));
            return PooledSet<T>(data);
        }

        // outln("hashed: {, }", PooledSet<T>(*i));
        arena_.deallocate(state);
        return PooledSet<T>(*i);
    }

    [[nodiscard]] PooledSet<T> erase(PooledSet<T> set, const T& elem) {
        if (!set) return set;
        // outln("--\nerase({, } - {})", set, elem);
        auto i = binary_find(set.begin(), set.end(), elem, GIDLt<T>());
        if (i == set.end()) {
            // outln("not found: {}", set);
            return set;
        }

        auto size = set.size() - 1;
        if (size == 0) return PooledSet<T>(); // Empty Set is not hashed

        auto bytes = sizeof(typename PooledSet<T>::Data) + size * SizeOf<T>;
        auto state = arena_.state();
        auto buf   = arena_.allocate(bytes);
        auto data  = new (buf) typename PooledSet<T>::Data(size);

        std::copy(i + 1, set.end(), std::copy(set.begin(), i, data->elems)); // copy over, skip i

        auto [j, ins] = pool_.emplace(data);
        if (ins) {
            // outln("fresh: {, }", PooledSet<T>(data));
            return PooledSet<T>(data);
        }
        arena_.deallocate(state);
        // outln("hashed: {}", PooledSet<T>(*j));
        return PooledSet<T>(*j);
    }

    friend void swap(Pool& p1, Pool& p2) {
        using std::swap;
        swap(p1.arena_, p2.arena_);
        swap(p1.pool_, p2.pool_);
    }

private:
    template<class I, class A> void copy_and_inc_unique(I& i, A& ai) {
        if ((*i)->gid() != (*ai)->gid()) *++i = *ai;
        ++ai;
    }

    fe::Arena arena_;
    absl::flat_hash_set<const typename PooledSet<T>::Data*,
                        absl::Hash<const typename PooledSet<T>::Data*>,
                        typename PooledSet<T>::Data::Equal>
        pool_;
};
}
