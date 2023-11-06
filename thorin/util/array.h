#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

namespace thorin {

template<class T> class Array;

//------------------------------------------------------------------------------

/// A container-like wrapper for an array.
/// The array may either stem from a C array, a `std::vector`, a `std::initializer_list`, an Array or another Span.
/// Span does **not** own the data and, thus, does not destroy any data.
/// Likewise, you must be carefull to not destroy data a Span is pointing to.
/// Note that you can often construct a Span inline with an initializer_list: `foo(arg1, {elem1, elem2, elem3}, arg3)`.
template<class T> class Span {
public:
    using value_type             = T;
    using const_iterator         = const T*;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @name Constructor, Destructor & Assignment
    ///@{
    Span() noexcept               = default;
    Span(const Span<T>&) noexcept = default;
    Span(Span&& span) noexcept    = default;
    Span(size_t size, const T* ptr)
        : size_(size)
        , ptr_(ptr) {}
    template<size_t N>
    Span(const T (&ptr)[N])
        : size_(N)
        , ptr_(ptr) {}
    Span(const Array<T>& array)
        : size_(array.size())
        , ptr_(array.begin()) {}
    template<size_t N>
    Span(const std::array<T, N>& array)
        : size_(N)
        , ptr_(array.data()) {}
    Span(std::initializer_list<T> list)
        : size_(std::ranges::distance(list))
        , ptr_(std::begin(list)) {}
    Span(const std::vector<T>& vector)
        : size_(vector.size())
        , ptr_(vector.data()) {}
    Span& operator=(Span other) noexcept { return swap(*this, other), *this; }
    ///@}

    /// @name size
    ///@{
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    ///@}

    /// @name begin/end iterators
    ///@{
    const_iterator begin() const { return ptr_; }
    const_iterator end() const { return ptr_ + size_; }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    ///@}

    /// @name access
    ///@{
    const T& operator[](size_t i) const {
        assert(i < size() && "index out of bounds");
        return *(ptr_ + i);
    }
    T const& front() const {
        assert(!empty());
        return ptr_[0];
    }
    T const& back() const {
        assert(!empty());
        return ptr_[size_ - 1];
    }
    ///@}

    /// @name slice
    ///@{
    Span<T> skip_front(size_t num = 1) const { return Span<T>(size() - num, ptr_ + num); }
    Span<T> skip_back(size_t num = 1) const { return Span<T>(size() - num, ptr_); }
    Span<T> first(size_t num = 1) const {
        assert(num <= size());
        return Span<T>(num, ptr_);
    }
    Span<T> last(size_t num = 1) const {
        assert(num <= size());
        return Span<T>(num, ptr_ + size() - num);
    }
    ///@}

    /// @name relational operators
    ///@{
    template<class Other> bool operator==(const Other& other) const {
        return this->size() == other.size() && std::equal(begin(), end(), other.begin());
    }
    template<class Other> bool operator!=(const Other& other) const {
        return this->size() != other.size() || !std::equal(begin(), end(), other.begin());
    }
    ///@}

    /// Convert to `std::array`.
    template<size_t N> std::array<T, N> to_array() const {
        assert(size() == N);
        std::array<T, N> result;
        std::ranges::copy(*this, result.begin());
        return result;
    }

    friend void swap(Span<T>& a1, Span<T>& a2) noexcept {
        using std::swap;
        swap(a1.size_, a2.size_);
        swap(a1.ptr_, a2.ptr_);
    }

private:
    size_t size_  = 0;
    const T* ptr_ = nullptr;
};

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
    Array(Span<T> ref, std::function<T(T)> f)
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
    Span<T> skip_front(size_t num = 1) const { return Span<T>(size() - num, data() + num); }
    Span<T> skip_back(size_t num = 1) const { return Span<T>(size() - num, data()); }
    Span<T> first(size_t num = 1) const {
        assert(num <= size());
        return Span<T>(num, data());
    }
    Span<T> last(size_t num = 1) const {
        assert(num <= size());
        return Span<T>(num, data() + size() - num);
    }
    ///@}

    /// @name convert
    ///@{
    Span<T> ref() const { return Span<T>(size(), data()); }
    template<size_t N> std::array<T, N> to_array() const { return ref().template to_array<N>(); }
    ///@}

    /// @name relational operators
    ///@{
    bool operator==(const Array other) const { return Span<T>(*this) == Span<T>(other); }
    bool operator!=(const Array other) const { return Span<T>(*this) != Span<T>(other); }
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
