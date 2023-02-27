#pragma once

#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_set.h>

namespace thorin {

class Sym {
public:
    Sym() = default;

    bool is_anonymous() const { return ptr_ && ptr_->size() == 1 && ptr_->front() == '_'; }
    bool operator==(Sym other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym other) const { return this->ptr_ != other.ptr_; }
    operator const std::string_view() const { return ptr_ ? *ptr_ : std::string_view(); }
    operator const std::string&() const { return ptr_ ? *ptr_ : empty; }
    explicit operator bool() const { return ptr_; }
    std::string_view operator*() const { return (std::string_view)(*this); }
    const std::string* operator->() const { return ptr_ ? ptr_ : &empty; }
    std::string str() const { return ptr_ ? *ptr_ : empty; }
    char operator[](size_t i) const { return ((std::string_view)(*this))[i]; }

private:
    Sym(const std::string* ptr)
        : ptr_(ptr) {
        assert(ptr == nullptr || !ptr->empty());
    }

    static constexpr std::string empty = {};
    const std::string* ptr_ = nullptr;

    template<class H>
    friend H AbslHashValue(H h, Sym sym) {
        return H::combine(std::move(h), sym.ptr_);
    }

    friend class SymPool;
};

inline std::ostream& operator<<(std::ostream& o, const Sym sym) { return o << *sym; }

class SymPool {
public:
    SymPool()            = default;
    SymPool(const SymPool&) = delete;
    SymPool(SymPool&& other)
        : pool_(std::move(other.pool_)) {}

    Sym sym(std::string_view s) { return s.empty() ? Sym() : &*pool_.emplace(s).first; }
    Sym sym(const char* s) { return s == nullptr || *s == '\0'  ? Sym() : &*pool_.emplace(s).first; }
    Sym sym(std::string&& s) { return s.empty() ? Sym() : &*pool_.emplace(std::move(s)).first; }

    friend void swap(SymPool& p1, SymPool& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

template<class V>
using SymMap = absl::flat_hash_map<Sym, V>;
using SymSet = absl::flat_hash_set<Sym>;
using Syms   = std::deque<Sym>;

inline std::string_view sub_view(std::string_view s, size_t i, size_t n = std::string_view::npos) {
    n = std::min(n, s.size());
    return {s.data() + i, n - i};
}

} // namespace thorin
