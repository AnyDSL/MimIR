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

template<class T>
class Array;

//------------------------------------------------------------------------------

/// A container-like wrapper for an array.
/// The array may either stem from a C array, a `std::vector`, a `std::initializer_list`, an Array or another ArrayRef.
/// ArrayRef does **not** own the data and, thus, does not destroy any data.
/// Likewise, you must be carefull to not destroy data an ArrayRef is pointing to.
/// Thorin makes use of ArrayRef%s in many places.
/// Note that you can often construct an ArrayRef inline with an initializer_list: `foo(arg1, {elem1, elem2, elem3},
/// arg3)`. Useful operations are ArrayRef::skip_front and ArrayRef::skip_back to create other ArrayRef%s.
template<class T>
class ArrayRef {
public:
    using value_type             = T;
    using const_iterator         = const T*;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @name constructor, destructor & assignment
    ///@{
    ArrayRef()
        : size_(0)
        , ptr_(nullptr) {}
    ArrayRef(size_t size, const T* ptr)
        : size_(size)
        , ptr_(ptr) {}
    template<size_t N>
    ArrayRef(const T (&ptr)[N])
        : size_(N)
        , ptr_(ptr) {}
    ArrayRef(const ArrayRef<T>& ref)
        : size_(ref.size_)
        , ptr_(ref.ptr_) {}
    ArrayRef(const Array<T>& array)
        : size_(array.size())
        , ptr_(array.begin()) {}
    template<size_t N>
    ArrayRef(const std::array<T, N>& array)
        : size_(N)
        , ptr_(array.data()) {}
    ArrayRef(std::initializer_list<T> list)
        : size_(std::ranges::distance(list))
        , ptr_(std::begin(list)) {}
    ArrayRef(const std::vector<T>& vector)
        : size_(vector.size())
        , ptr_(vector.data()) {}
    ArrayRef(ArrayRef&& array)
        : ArrayRef() {
        swap(*this, array);
    }

    ArrayRef& operator=(ArrayRef other) {
        swap(*this, other);
        return *this;
    }
    template<size_t N>
    std::array<T, N> to_array() const {
        assert(size() == N);
        std::array<T, N> result;
        std::ranges::copy(*this, result.begin());
        return result;
    }
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
    ArrayRef<T> skip_front(size_t num = 1) const { return ArrayRef<T>(size() - num, ptr_ + num); }
    ArrayRef<T> skip_back(size_t num = 1) const { return ArrayRef<T>(size() - num, ptr_); }
    ArrayRef<T> get_front(size_t num = 1) const {
        assert(num <= size());
        return ArrayRef<T>(num, ptr_);
    }
    ArrayRef<T> get_back(size_t num = 1) const {
        assert(num <= size());
        return ArrayRef<T>(num, ptr_ + size() - num);
    }
    Array<T> cut(ArrayRef<size_t> indices, size_t reserve = 0) const;
    ///@}

    /// @name relational operators
    ///@{
    template<class Other>
    bool operator==(const Other& other) const {
        return this->size() == other.size() && std::equal(begin(), end(), other.begin());
    }
    template<class Other>
    bool operator!=(const Other& other) const {
        return this->size() != other.size() || !std::equal(begin(), end(), other.begin());
    }
    ///@}

    friend void swap(ArrayRef<T>& a1, ArrayRef<T>& a2) {
        using std::swap;
        swap(a1.size_, a2.size_);
        swap(a1.ptr_, a2.ptr_);
    }

private:
    size_t size_;
    const T* ptr_;
};

//------------------------------------------------------------------------------

template<typename T, size_t StackSize>
class ArrayStorage {
    // Unions only work with trivial types.
    static_assert(std::is_trivial<T>::value, "Stack based array storage is only available for trivial types");

public:
    ArrayStorage()
        : size_(0)
        , stack_(true) {}
    ArrayStorage(size_t size) {
        size_  = size;
        stack_ = size <= StackSize;
        if (!stack_) data_.ptr = new T[size]();
    }
    ArrayStorage(ArrayStorage&& other)
        : size_(other.size_)
        , stack_(other.stack_)
        , data_(std::move(other.data_)) {
        other.stack_ = true;
        other.size_  = 0;
    }

    ~ArrayStorage() {
        if (!stack_) delete[] data_.ptr;
    }

    size_t size() const { return size_; }
    void shrink(size_t new_size) { size_ = new_size; }
    T* data() { return stack_ ? data_.elems : data_.ptr; }
    const T* data() const { return stack_ ? data_.elems : data_.ptr; }

    friend void swap(ArrayStorage& a, ArrayStorage& b) {
        auto size  = a.size_;
        a.size_    = b.size_;
        b.size_    = size;
        auto stack = a.stack_;
        a.stack_   = b.stack_;
        b.stack_   = stack;
        std::swap(a.data_, b.data_);
    }

private:
    size_t size_ : sizeof(size_t) * 8 - 1;
    bool stack_ : 1;
    union {
        T* ptr;
        T elems[StackSize];
    } data_;
};

template<typename T>
struct ArrayStorage<T, 0> {
public:
    ArrayStorage()
        : size_(0)
        , ptr_(nullptr) {}
    ArrayStorage(size_t size)
        : size_(size)
        , ptr_(new T[size]) {}
    ArrayStorage(ArrayStorage&& other)
        : size_(std::move(other.size_))
        , ptr_(std::move(other.ptr_)) {
        other.ptr_  = nullptr;
        other.size_ = 0;
    }
    ~ArrayStorage() { delete[] ptr_; }

    size_t size() const { return size_; }
    void shrink(size_t new_size) { size_ = new_size; }
    T* data() { return ptr_; }
    const T* data() const { return ptr_; }

    friend void swap(ArrayStorage& a, ArrayStorage& b) {
        std::swap(a.size_, b.size_);
        std::swap(a.ptr_, b.ptr_);
    }

private:
    size_t size_;
    T* ptr_;
};

//------------------------------------------------------------------------------

/// A container for an array, either heap-allocated or stack allocated.
/// This class is similar to `std::vector` with the following differences:
/// * If the size is small enough, the array resides on the stack.
/// * In contrast to std::vector, Array cannot grow dynamically.
///   An Array may Array::shrink, however.
///   But once shrunk, there is no way back.
template<class T>
class Array {
public:
    using value_type             = T;
    using iterator               = T*;
    using const_iterator         = const T*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// @name constructor, destructor & assignment
    ///@{
    Array()
        : storage_(0) {}
    explicit Array(size_t size)
        : storage_(size) {
        for (auto& elem : *this) new (&elem) T();
    }
    Array(size_t size, const T& val)
        : storage_(size) {
        std::ranges::fill(*this, val);
    }
    Array(ArrayRef<T> ref)
        : storage_(ref.size()) {
        std::ranges::copy(ref, begin());
    }
    Array(Array&& other)
        : storage_(std::move(other.storage_)) {}
    Array(const Array& other)
        : storage_(other.size()) {
        std::ranges::copy(other, begin());
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

    Array& operator=(Array other) {
        swap(*this, other);
        return *this;
    }
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
    ArrayRef<T> skip_front(size_t num = 1) const { return ArrayRef<T>(size() - num, data() + num); }
    ArrayRef<T> skip_back(size_t num = 1) const { return ArrayRef<T>(size() - num, data()); }
    ArrayRef<T> get_front(size_t num = 1) const {
        assert(num <= size());
        return ArrayRef<T>(num, data());
    }
    ArrayRef<T> get_back(size_t num = 1) const {
        assert(num <= size());
        return ArrayRef<T>(num, data() + size() - num);
    }
    Array<T> cut(ArrayRef<size_t> indices, size_t reserve = 0) const {
        return ArrayRef<T>(*this).cut(indices, reserve);
    }
    ///@}

    /// @name convert
    ///@{
    ArrayRef<T> ref() const { return ArrayRef<T>(size(), data()); }
    template<size_t N>
    std::array<T, N> to_array() const {
        return ref().template to_array<N>();
    }
    ///@}

    /// @name relational operators
    ///@{
    bool operator==(const Array other) const { return ArrayRef<T>(*this) == ArrayRef<T>(other); }
    bool operator!=(const Array other) const { return ArrayRef<T>(*this) != ArrayRef<T>(other); }
    ///@}

    friend void swap(Array& a, Array& b) { swap(a.storage_, b.storage_); }

private:
    ArrayStorage<T, std::is_trivial<T>::value ? 5 : 0> storage_;
};

template<class T>
Array<T> ArrayRef<T>::cut(ArrayRef<size_t> indices, size_t reserve) const {
    size_t num_old = size();
    size_t num_idx = indices.size();
    size_t num_res = num_old - num_idx;

    Array<T> result(num_res + reserve);

    for (size_t o = 0, i = 0, r = 0; o < num_old; ++o) {
        if (i < num_idx && indices[i] == o)
            ++i;
        else
            result[r++] = (*this)[o];
    }

    return result;
}

template<class T, class U>
auto concat(const T& a, const U& b) -> Array<typename T::value_type> {
    Array<typename T::value_type> result(a.size() + b.size());
    std::ranges::copy(b, std::ranges::copy(a, result.begin()));
    return result;
}

template<class T>
auto concat(const T& val, ArrayRef<T> a) -> Array<T> {
    Array<T> result(a.size() + 1);
    std::ranges::copy(a, result.begin() + 1);
    result.front() = val;
    return result;
}

template<class T>
auto concat(ArrayRef<T> a, const T& val) -> Array<T> {
    Array<T> result(a.size() + 1);
    std::ranges::copy(a, result.begin());
    result.back() = val;
    return result;
}

template<class T>
Array<typename T::value_type> make_array(const T& container) {
    return Array<typename T::value_type>(container.begin(), container.end());
}

} // namespace thorin
