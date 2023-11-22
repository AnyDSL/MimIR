#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace thorin {

template<class T> class Array;

/// Something which behaves like `std::vector` or `std::array`.
template<class Vec>
concept Vectorlike = requires(Vec vec) {
    typename Vec::value_type;
    vec.size();
    vec.data();
};

/// This is a thin wrapper for `std::span<T, N>` with the following additional features:
/// * Constructor with std::initializer_list list (C++26 will get this ...)
/// * Constructor for any compatible Vectorlike argument
/// * rsubspan (reverse subspan)
/// * structured binding, if `N ! std::dynamic_extent:
/// ```
/// Span<int, 3> span = /*...*/;
/// auto& [a, b, c] = span;
/// b = 23;
/// ```
template<class T, size_t N = std::dynamic_extent> class Span : public std::span<T, N> {
public:
    using Base = std::span<T, N>;

    /// @name Constructors
    ///@{
    using Base::Base;
    Span(std::span<T, N> span)
        : Base(span) {}
    Span(std::initializer_list<T> list)
        : Base(std::begin(list), std::ranges::distance(list)) {}
    template<Vectorlike Vec>
    requires(std::is_same_v<typename Vec::value_type, T>)
    Span(Vec& vec)
        : Base(vec.data(), vec.size()) {}
    template<Vectorlike Vec>
    requires(std::is_same_v<std::add_const_t<typename Vec::value_type>, std::add_const_t<T>>)
    Span(const Vec& vec)
        : Base(vec.data(), vec.size()) {}
    constexpr Span(typename Base::pointer p)
        : Base(p, N) {}
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
            return {Base::data() + Base::size() - i - n}; // size: n
        else if constexpr (N != std::dynamic_extent)
            return {Base::data()}; // size: N - i
        else
            return {Base::data(), Base::size() - i};
    }
    ///@}
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
template<class T, size_t N> struct tuple_size<thorin::Span<T, N>> : std::integral_constant<size_t, N> {};

template<size_t I, class T, size_t N> struct tuple_element<I, thorin::Span<T, N>> {
    using type = typename thorin::Span<T, N>::reference;
};

template<size_t I, class T, size_t N> constexpr decltype(auto) get(thorin::Span<T, N> span) { return (span[I]); }
} // namespace std

namespace thorin {

template<class R, class S> bool equal(R range1, S range2) {
    if (range1.size() != range2.size()) return false;
    auto j = range2.begin();
    for (auto i = range1.begin(), e = range1.end(); i != e; ++i, ++j)
        if (*i != *j) return false;
    return true;
}

//------------------------------------------------------------------------------

template<typename T, size_t StackSize> class ArrayStorage {
    // Unions only work with trivial types.
    static_assert(std::is_trivial<T>::value, "Stack based array storage is only available for trivial types");

public:
    /// @name Constructor, Destructor & Assignment
    ///@{
    ArrayStorage() noexcept
        : size_(0)
        , stack_(true) {}
    ArrayStorage(size_t size) {
        size_  = size;
        stack_ = size <= StackSize;
        if (!stack_) data_.ptr = new T[size]();
    }
    ArrayStorage(ArrayStorage&& other) noexcept
        : ArrayStorage() {
        swap(*this, other);
    }
    ~ArrayStorage() {
        if (!stack_) delete[] data_.ptr;
    }
    ArrayStorage& operator=(ArrayStorage other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name Getters
    ///@{
    size_t size() const { return size_; }
    void shrink(size_t new_size) { size_ = new_size; }
    T* data() { return stack_ ? data_.elems : data_.ptr; }
    const T* data() const { return stack_ ? data_.elems : data_.ptr; }
    ///@}

    friend void swap(ArrayStorage& a, ArrayStorage& b) noexcept {
        using std::swap;
        auto size  = a.size_;
        a.size_    = b.size_;
        b.size_    = size;
        auto stack = a.stack_;
        a.stack_   = b.stack_;
        b.stack_   = stack;
        swap(a.data_, b.data_);
    }

private:
    size_t size_ : sizeof(size_t) * 8 - 1;
    bool stack_ : 1;
    union {
        T* ptr;
        T elems[StackSize];
    } data_;
};

template<typename T> struct ArrayStorage<T, 0> {
public:
    /// @name Constructor, Destructor & Assignment
    ///@{
    ArrayStorage() noexcept = default;
    ArrayStorage(size_t size)
        : size_(size)
        , ptr_(new T[size]) {}
    ArrayStorage(ArrayStorage&& other) noexcept
        : ArrayStorage() {
        swap(*this, other);
    }
    ~ArrayStorage() { delete[] ptr_; }
    ArrayStorage& operator=(ArrayStorage other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name Getters
    ///@{
    size_t size() const { return size_; }
    void shrink(size_t new_size) { size_ = new_size; }
    T* data() { return ptr_; }
    const T* data() const { return ptr_; }
    ///@}

    friend void swap(ArrayStorage& a, ArrayStorage& b) noexcept {
        using std::swap;
        // clang-format off
        swap(a.size_, b.size_);
        swap(a.ptr_,  b.ptr_);
        // clang-format on
    }

private:
    size_t size_ = 0;
    T* ptr_      = nullptr;
};

//------------------------------------------------------------------------------

/// A container for an array, either heap-allocated or stack allocated.
/// This class is similar to `std::vector` with the following differences:
/// * If the size is small enough, the array resides on the stack.
/// * In contrast to std::vector, Array cannot grow dynamically.
///   An Array may Array::shrink, however.
///   But once shrunk, there is no way back.
template<class T> class Array {
public:
    using value_type             = T;
    using iterator               = T*;
    using const_iterator         = const T*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @name Constructor, Destructor & Assignment
    ///@{
    Array() noexcept
        : storage_(0) {}
    Array(const Array& other)
        : storage_(other.size()) {
        std::ranges::copy(other, begin());
    }
    Array(Array&& other) noexcept
        : Array() {
        swap(*this, other);
    }
    explicit Array(size_t size)
        : storage_(size) {
        for (auto& elem : *this) new (&elem) T();
    }
    Array(size_t size, const T& val)
        : storage_(size) {
        std::ranges::fill(*this, val);
    }
    Array(Span<T> ref)
        : storage_(ref.size()) {
        std::ranges::copy(ref, begin());
    }
    Array(View<T> ref)
        : storage_(ref.size()) {
        std::ranges::copy(ref, begin());
    }
    Array(const std::vector<T>& other)
        : storage_(other.size()) {
        std::ranges::copy(other, begin());
    }
    template<class I>
    Array(const I begin, const I end)
        : storage_(std::distance(begin, end)) {
        std::copy(begin, end, data());
    }
    Array(std::initializer_list<T> list)
        : storage_(std::ranges::distance(list)) {
        std::ranges::copy(list, data());
    }
    Array(size_t size, std::function<T(size_t)> f)
        : storage_(size) {
        for (size_t i = 0; i != size; ++i) (*this)[i] = f(i);
    }
    template<class U, class F>
    Array(Span<U> ref, F f)
        : storage_(ref.size()) {
        for (size_t i = 0, e = ref.size(); i != e; ++i) (*this)[i] = f(ref[i]);
    }
    Array& operator=(Array other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name size
    ///@{
    size_t size() const { return storage_.size(); }
    bool empty() const { return size() == 0; }
    void shrink(size_t new_size) {
        assert(new_size <= size());
        storage_.shrink(new_size);
    }
    ///@}

    /// @name begin/end iterators
    ///@{
    iterator begin() { return data(); }
    iterator end() { return data() + size(); }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return data() + size(); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    ///@}

    /// @name access
    ///@{
    T& front() {
        assert(!empty());
        return data()[0];
    }
    T& back() {
        assert(!empty());
        return data()[size() - 1];
    }
    const T& front() const {
        assert(!empty());
        return data()[0];
    }
    const T& back() const {
        assert(!empty());
        return data()[size() - 1];
    }
    T* data() { return storage_.data(); }
    const T* data() const { return storage_.data(); }
    T& operator[](size_t i) {
        assert(i < size() && "index out of bounds");
        return data()[i];
    }
    T const& operator[](size_t i) const {
        assert(i < size() && "index out of bounds");
        return data()[i];
    }
    ///@}

    /// @name slice
    ///@{
    View<T> skip_front(size_t num = 1) const { return View<T>(data() + num, size() - num); }
    View<T> skip_back(size_t num = 1) const { return View<T>(data(), size() - num); }
    View<T> first(size_t num = 1) const {
        assert(num <= size());
        return View<T>(data(), num);
    }
    Span<T> last(size_t num = 1) const {
        assert(num <= size());
        return View<T>(data() + size() - num, num);
    }
    ///@}

    /// @name convert
    ///@{
    View<T> ref() const { return View<T>(size(), data()); }
    ///@}

    friend void swap(Array& a, Array& b) noexcept {
        using std::swap;
        swap(a.storage_, b.storage_);
    }

private:
    ArrayStorage<T, std::is_trivial<T>::value ? 5 : 0> storage_;
};

/// @name Array/ArrayRef Helpers
///@{
template<class T, class U> auto concat(const T& a, const U& b) -> Array<typename T::value_type> {
    Array<typename T::value_type> result(a.size() + b.size());
    std::ranges::copy(b, std::ranges::copy(a, result.begin()));
    return result;
}

template<class T> auto concat(const T& val, Span<T> a) -> Array<T> {
    Array<T> result(a.size() + 1);
    std::ranges::copy(a, result.begin() + 1);
    result.front() = val;
    return result;
}

template<class T> auto concat(Span<T> a, const T& val) -> Array<T> {
    Array<T> result(a.size() + 1);
    std::ranges::copy(a, result.begin());
    result.back() = val;
    return result;
}

template<class T> Array<typename T::value_type> make_array(const T& container) {
    return Array<typename T::value_type>(container.begin(), container.end());
}
///@}

} // namespace thorin
