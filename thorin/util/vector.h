#pragma once

#include <algorithm>

#include <absl/container/inlined_vector.h>

namespace thorin {

template<class T> static constexpr size_t Default_Inlined_Size = std::max((size_t)1, 4 * sizeof(size_t) / sizeof(T));
template<class T> using Vector                                 = absl::InlinedVector<T, Default_Inlined_Size<T>>;

template<class R, class S> bool equal(R range1, S range2) {
    if (range1.size() != range2.size()) return false;
    auto j = range2.begin();
    for (auto i = range1.begin(), e = range1.end(); i != e; ++i, ++j)
        if (*i != *j) return false;
    return true;
}

template<class T, class F, size_t N = Default_Inlined_Size<T>> absl::InlinedVector<T, N> vector(size_t size, F f) {
    auto result = absl::InlinedVector<T, N>(size);
    for (size_t i = 0; i != size; ++i) result[i] = f(i);
    return result;
}

template<class T, std::ranges::range R, class F, size_t N = Default_Inlined_Size<T>> auto vector(const R& range, F f) {
    auto result = absl::InlinedVector<T, N>(std::ranges::distance(range));
    auto ri     = std::ranges::begin(range);
    for (size_t i = 0; i != result.size(); ++i, ++ri) result[i] = f(*ri);
    return result;
}

template<class T, size_t N, class A, class U>
constexpr typename absl::InlinedVector<T, N, A>::size_type erase(absl::InlinedVector<T, N, A>& c, const U& value) {
    auto it = std::remove(c.begin(), c.end(), value);
    auto r  = c.end() - it;
    c.erase(it, c.end());
    return r;
}

template<class T, size_t N, class A, class Pred>
constexpr typename absl::InlinedVector<T, N, A>::size_type erase_if(absl::InlinedVector<T, N, A>& c, Pred pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r  = c.end() - it;
    c.erase(it, c.end());
    return r;
}

} // namespace thorin
