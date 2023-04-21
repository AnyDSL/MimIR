#pragma once

#include <iostream>
#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_set.h>

namespace thorin {

/// A Sym%bol just wraps a `const std::string*`, so pass Sym itself around as value.
/// With the exception of the empty string, you should only create Sym%bols via SymPool::sym.
/// This in turn will toss all Sym%bols into a big hash set.
/// This makes Sym::operator== and Sym::operator!= an O(1) operation.
/// The empty string is internally handled as `nullptr`.
/// Thus, you can create a Sym%bol representing an empty string without having access to the SymPool.
/// The empty string, `nullptr`, and `"\0"` are all identified as Sym::Sym().
class Sym {
private:
    Sym(const std::string* ptr)
        : ptr_(ptr) {
        assert(ptr == nullptr || !ptr->empty());
    }

public:
    Sym() = default;

    /// @name begin/end
    ///@{
    /// Useful for range-based for.
    /// Will give you `std::string::const_iterator` - yes **const**; you are not supposed to mutate hashed strings.
    auto begin() const { return (*this)->cbegin(); }
    auto end() const { return (*this)->cend(); }
    ///@}

    /// @name Comparisons
    ///@{
    auto operator<=>(char c) const {
        if ((*this)->size() == 0) return std::strong_ordering::less;
        auto cmp = (*this)[0] <=> c;
        if ((*this)->size() == 1) return cmp;
        return cmp == 0 ? std::strong_ordering::greater : cmp;
    }
    auto operator==(char c) const { return (*this) <=> c == std::strong_ordering::equal; }
    auto operator<=>(Sym other) const { return **this <=> *other; }
    bool operator==(Sym other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym other) const { return this->ptr_ != other.ptr_; }
    ///@}

    /// @name Cast Operators
    ///@{
    operator std::string_view() const { return ptr_ ? *ptr_ : std::string_view(); }
    operator const std::string&() const { return *this->operator->(); }
    explicit operator bool() const { return ptr_; }
    ///@}

    /// @name Access Operators
    ///@{
    char operator[](size_t i) const { return ((const std::string&)(*this))[i]; }
    const std::string& operator*() const { return *this->operator->(); }
    const std::string* operator->() const {
        static std::string empty;
        return ptr_ ? ptr_ : &empty;
    }
    ///@}

    template<class H>
    friend H AbslHashValue(H h, Sym sym) {
        return H::combine(std::move(h), sym.ptr_);
    }

private:
    const std::string* ptr_ = nullptr;

    friend class SymPool;
};

/// @name std::ostream operator
///@{
inline std::ostream& operator<<(std::ostream& o, const Sym sym) { return o << *sym; }
///@}

/// Hash set where all strings - wrapped in Sym%bol - live in.
/// You can access the SymPool from Driver.
class SymPool {
public:
    SymPool()               = default;
    SymPool(const SymPool&) = delete;
    SymPool(SymPool&& other)
        : pool_(std::move(other.pool_)) {}

    /// @name sym
    ///@{
    Sym sym(std::string_view s) { return s.empty() ? Sym() : &*pool_.emplace(s).first; }
    Sym sym(const char* s) { return s == nullptr || *s == '\0' ? Sym() : &*pool_.emplace(s).first; }
    Sym sym(std::string s) { return s.empty() ? Sym() : &*pool_.emplace(std::move(s)).first; }
    ///@}

    friend void swap(SymPool& p1, SymPool& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

/// @name Sym
///@{
/// Set/Map is keyed by pointer - which is hashed in SymPool.
template<class V>
using SymMap = absl::flat_hash_map<Sym, V>;
using SymSet = absl::flat_hash_set<Sym>;
using Syms   = std::deque<Sym>;
///@}

} // namespace thorin
