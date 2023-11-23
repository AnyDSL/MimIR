#pragma once

#include <span>

namespace thorin {

/// Something which behaves like `std::vector` or `std::array`.
template<class Vec>
concept Vectorlike = requires(Vec vec) {
    typename Vec::value_type;
    vec.size();
    vec.data();
};

/// This is a thin wrapper for `std::span<T, N>` with the following additional features:
/// * Constructor with `std::initializer_list` (C++26 will get this ...)
/// * Constructor for any compatible Vectorlike argument
/// * rsubspan (reverse subspan)
/// * structured binding, if `N != std::dynamic_extent:`
/// ```
/// void f(Span<int, 3> span) {
///     auto& [a, b, c] = span;
///     b = 23;
///     // ...
/// }
/// ```
template<class T, size_t N = std::dynamic_extent> class Span : public std::span<T, N> {
public:
    using Base = std::span<T, N>;

    /// @name Constructors
    ///@{
    using Base::Base;
    explicit(N != std::dynamic_extent) constexpr Span(std::initializer_list<T> init) noexcept
        : Base(std::begin(init), std::ranges::distance(init)) {}
    constexpr Span(std::span<T, N> span) noexcept
        : Base(span) {}
    template<Vectorlike Vec>
    requires(std::is_same_v<typename Vec::value_type, T>)
    explicit(N != std::dynamic_extent) constexpr Span(Vec& vec)
        : Base(vec.data(), vec.size()) {}
    template<Vectorlike Vec>
    requires(std::is_same_v<std::add_const_t<typename Vec::value_type>, std::add_const_t<T>>)
    explicit(N != std::dynamic_extent) constexpr Span(const Vec& vec)
        : Base(vec.data(), vec.size()) {}
    constexpr explicit Span(typename Base::pointer p)
        : Base(p, N) {
        static_assert(N != std::dynamic_extent);
    }
    ///@}

    /// @name subspan
    ///@{
    /// Wrappers for `std::span::subspan` that return a `thorin::Span`.
    constexpr Span<T, std::dynamic_extent> subspan(size_t i, size_t n = std::dynamic_extent) const {
        return Base::subspan(i, n);
    }
    constexpr Span<T, std::dynamic_extent> rsubspan(size_t i, size_t n = std::dynamic_extent) const {
        return n != std::dynamic_extent ? subspan(Base::size() - i - n, n) : subspan(0, Base::size() - i);
    }
    ///@}

    /// @name rsubspan
    ///@{
    /// Similar to Span::subspan but in *reverse*:
    /// `span.rsubspan(3, 5)` removes the last 3 elements and while picking 5 elements onwards from there.
    /// E.g.: If `span` points to `0, 1, 2, 3, 4, 5, 6, 7, 8, 9`, then the result will point to `2, 3, 4, 5, 6`.
    template<size_t i, size_t n = std::dynamic_extent>
    constexpr Span<T, n != std::dynamic_extent ? n : (N != std::dynamic_extent ? N - i : std::dynamic_extent)>
    subspan() const {
        return Base::template subspan<i, n>();
    }

    template<size_t i, size_t n = std::dynamic_extent>
    constexpr Span<T, n != std::dynamic_extent ? n : (N != std::dynamic_extent ? N - i : std::dynamic_extent)>
    rsubspan() const {
        if constexpr (n != std::dynamic_extent)
            return Span<T, n>(Base::data() + Base::size() - i - n);
        else if constexpr (N != std::dynamic_extent)
            return Span<T, N - i>(Base::data());
        else
            return Span<T, std::dynamic_extent>(Base::data(), Base::size() - i);
    }
    ///@}

    friend void swap(Span& s1, Span& s2) { s1.swap(s2); }
};

template<class T, size_t N = std::dynamic_extent> using View = Span<const T, N>;

/// @name Deduction Guides
///@{
template<class I, class E> Span(I, E) -> Span<std::remove_reference_t<std::iter_reference_t<I>>>;
template<class T, size_t N> Span(T (&)[N]) -> Span<T, N>;
template<class T, size_t N> Span(std::array<T, N>&) -> Span<T, N>;
template<class T, size_t N> Span(const std::array<T, N>&) -> Span<const T, N>;
template<class R> Span(R&&) -> Span<std::remove_reference_t<std::ranges::range_reference_t<R>>>;
template<Vectorlike Vec> Span(Vec&) -> Span<typename Vec::value_type, std::dynamic_extent>;
template<Vectorlike Vec> Span(const Vec&) -> Span<const typename Vec::value_type, std::dynamic_extent>;
///@}

} // namespace thorin

namespace std {
/// @name Structured Binding Support for Span
///@{
template<class T, size_t N> struct tuple_size<thorin::Span<T, N>> : std::integral_constant<size_t, N> {};

template<size_t I, class T, size_t N> struct tuple_element<I, thorin::Span<T, N>> {
    using type = typename thorin::Span<T, N>::reference;
};

template<size_t I, class T, size_t N> constexpr decltype(auto) get(thorin::Span<T, N> span) { return (span[I]); }
///@}
} // namespace std
