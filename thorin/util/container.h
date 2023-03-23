#pragma once

#include <optional>
#include <queue>
#include <stack>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/node_hash_set.h>

#include "thorin/util/assert.h"
#include "thorin/util/hash.h"

namespace thorin {

template<class S>
auto pop(S& s) -> decltype(s.top(), typename S::value_type()) {
    auto val = s.top();
    s.pop();
    return val;
}

template<class Q>
auto pop(Q& q) -> decltype(q.front(), typename Q::value_type()) {
    auto val = q.front();
    q.pop();
    return val;
}

template<class I, class T, class Cmp>
I binary_find(I begin, I end, T val, Cmp cmp) {
    auto i = std::lower_bound(begin, end, val, cmp);
    return (i != end && !(cmp(val, *i))) ? i : end;
}

template<class Set>
class unique_stack {
public:
    using T = typename std::remove_reference_t<Set>::value_type;

    bool push(T val) {
        if (done_.emplace(val).second) {
            stack_.emplace(val);
            return true;
        }
        return false;
    }

    bool empty() const { return stack_.empty(); }
    const T& top() { return stack_.top(); }
    T pop() { return thorin::pop(stack_); }
    void clear() {
        done_.clear();
        stack_ = {};
    }

private:
    Set done_;
    std::stack<T> stack_;
};

template<class Set>
class unique_queue {
public:
    using T = typename std::remove_reference_t<Set>::value_type;

    unique_queue() = default;
    unique_queue(Set set)
        : done_(set) {}

    bool push(T val) {
        if (done_.emplace(val).second) {
            queue_.emplace(val);
            return true;
        }
        return false;
    }

    bool empty() const { return queue_.empty(); }
    T pop() { return thorin::pop(queue_); }
    T& front() { return queue_.front(); }
    T& back() { return queue_.back(); }
    void clear() {
        done_.clear();
        queue_ = {};
    }

private:
    Set done_;
    std::queue<T> queue_;
};

/// @name maps/sets based upon gid
///@{
template<class T>
struct GIDHash {
    size_t operator()(T p) const { return murmur3(p->gid()); };
};

template<class T>
struct GIDEq {
    bool operator()(T a, T b) const { return a->gid() == b->gid(); }
};

template<class T>
struct GIDLt {
    bool operator()(T a, T b) const { return a->gid() < b->gid(); }
};

// clang-format off
template<class K, class V> using GIDMap     = absl::flat_hash_map<K, V, GIDHash<K>, GIDEq<K>>;
template<class K>          using GIDSet     = absl::flat_hash_set<K,    GIDHash<K>, GIDEq<K>>;
template<class K, class V> using GIDNodeMap = absl::node_hash_map<K, V, GIDHash<K>, GIDEq<K>>;
template<class K>          using GIDNodeSet = absl::node_hash_set<K,    GIDHash<K>, GIDEq<K>>;
// clang-format on
///@}

/// Yields pointer to element (or the element itself if it is already a pointer), if found and `nullptr` otherwise.
/// @warning If the element is **not** already a pointer, this lookup will simply take the address of this element.
/// This means that, e.g., a rehash of an `absl::flat_hash_map` will invalidate this pointer.
template<class C, class K>
auto lookup(const C& container, const K& key) {
    auto i = container.find(key);
    if constexpr (std::is_pointer_v<typename C::mapped_type>)
        return i != container.end() ? i->second : nullptr;
    else
        return i != container.end() ? &i->second : nullptr;
}

/// Invokes `emplace` on @p container, asserts that insertion actually happened, and returns the iterator.
template<class C, class... Args>
auto assert_emplace(C& container, Args&&... args) {
    auto [i, ins] = container.emplace(std::forward<Args&&>(args)...);
    assert_unused(ins);
    return i;
}

} // namespace thorin
