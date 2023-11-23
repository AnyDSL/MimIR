#pragma once

#include <absl/container/inlined_vector.h>
#include <bits/ranges_base.h>

#include "thorin/util/span.h"

namespace thorin {

template<class T> static constexpr size_t Default_Inlined_Size = std::max((size_t)1, 4 * sizeof(size_t) / sizeof(T));

template<class T, size_t N = Default_Inlined_Size<T>, class A = std::allocator<T>>
class Vector : public absl::InlinedVector<T, N, A> {
public:
    using Base = absl::InlinedVector<T, N, A>;

    /// @name Constructors
    ///@{
    using Base::Base;
    template<class F>
    constexpr explicit Vector(size_t size, F&& f) requires(std::is_invocable_r_v<T, F, size_t>)
        : Base(size) {
        for (size_t i = 0; i != size; ++i) (*this)[i] = std::invoke(f, i);
    }
    template<std::ranges::forward_range R, class F>
    constexpr explicit Vector(const R& range, F&& f)
        requires(std::is_invocable_r_v<T, F, decltype(*std::ranges::begin(range))>)
        : Base(std::ranges::distance(range)) {
        auto ri = std::ranges::begin(range);
        for (auto& elem : *this) elem = std::invoke(f, *ri++);
    }
    ///@}

    /// @name Span
    ///@{
    Span<T> span() { return Span(Base::data(), Base::size()); }
    Span<const T> span() const { return Span(Base::data(), Base::size()); }
    Span<const T> view() const { return Span(Base::data(), Base::size()); }
    ///@}

    friend void swap(Vector& v1, Vector& v2) noexcept(noexcept(v1.swap(v2))) { v1.swap(v2); }
};

static_assert(std::ranges::contiguous_range<Vector<int>>);

/// @name Deduction Guides
///{@
template<class I, class A = std::allocator<typename std::iterator_traits<I>::value_type>>
Vector(I, I, A = A()) -> Vector<typename std::iterator_traits<I>::value_type,
                                Default_Inlined_Size<typename std::iterator_traits<I>::value_type>,
                                A>;
///}@

template<class R, class S> bool equal(R range1, S range2) {
    if (range1.size() != range2.size()) return false;
    auto j = range2.begin();
    for (auto i = range1.begin(), e = range1.end(); i != e; ++i, ++j)
        if (*i != *j) return false;
    return true;
}

template<class T, size_t N, class A, class U>
constexpr typename Vector<T, N, A>::size_type erase(Vector<T, N, A>& c, const U& value) {
    auto it = std::remove(c.begin(), c.end(), value);
    auto r  = c.end() - it;
    c.erase(it, c.end());
    return r;
}

template<class T, size_t N, class A, class Pred>
constexpr typename Vector<T, N, A>::size_type erase_if(Vector<T, N, A>& c, Pred pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r  = c.end() - it;
    c.erase(it, c.end());
    return r;
}

} // namespace thorin
