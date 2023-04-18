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

/// @name Utility Functions
///@{

template<class This, class T>
inline T& lazy_init(const This* self, std::unique_ptr<T>& ptr) {
    return *(ptr ? ptr : ptr = std::make_unique<T>(*self));
}

/// A bitcast from @p src of type @p S to @p D.
template<class D, class S>
inline D bitcast(const S& src) {
    D dst;
    auto s = reinterpret_cast<const void*>(&src);
    auto d = reinterpret_cast<void*>(&dst);

    if constexpr (sizeof(D) == sizeof(S)) std::memcpy(d, s, sizeof(D));
    if constexpr (sizeof(D) < sizeof(S)) std::memcpy(d, s, sizeof(D));
    if constexpr (sizeof(D) > sizeof(S)) {
        std::memset(d, 0, sizeof(D));
        std::memcpy(d, s, sizeof(S));
    }
    return dst;
}

template<class T>
bool get_sign(T val) {
    static_assert(std::is_integral<T>(), "get_sign only supported for signed and unsigned integer types");
    if constexpr (std::is_signed<T>())
        return val < 0;
    else
        return val >> (T(sizeof(val)) * T(8) - T(1));
}

// TODO I guess we can do that with C++20 <bit>
inline u64 pad(u64 offset, u64 align) {
    auto mod = offset % align;
    if (mod != 0) offset += align - mod;
    return offset;
}
///@}

/// @name Algorithms
///@{
template<class I, class T, class Cmp>
I binary_find(I begin, I end, T val, Cmp cmp) {
    auto i = std::lower_bound(begin, end, val, cmp);
    return (i != end && !(cmp(val, *i))) ? i : end;
}

/// Like `std::string::substr`, but works on `std::string_view` instead.
inline std::string_view subview(std::string_view s, size_t i, size_t n = std::string_view::npos) {
    n = std::min(n, s.size());
    return {s.data() + i, n - i};
}

/// Replaces all occurrences of @p what with @p repl.
inline void find_and_replace(std::string& str, std::string_view what, std::string_view repl) {
    for (size_t pos = str.find(what); pos != std::string::npos; pos = str.find(what, pos + repl.size()))
        str.replace(pos, what.size(), repl);
}
///@}

/// @name Helpers for Containers
///@{
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
///@}

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

/// @name GID
///@{
// clang-format off
template<class K, class V> using GIDMap     = absl::flat_hash_map<K, V, GIDHash<K>, GIDEq<K>>;
template<class K>          using GIDSet     = absl::flat_hash_set<K,    GIDHash<K>, GIDEq<K>>;
template<class K, class V> using GIDNodeMap = absl::node_hash_map<K, V, GIDHash<K>, GIDEq<K>>;
template<class K>          using GIDNodeSet = absl::node_hash_set<K,    GIDHash<K>, GIDEq<K>>;
// clang-format on
///@}

} // namespace thorin
