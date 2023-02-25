#pragma once

#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_set.h>

namespace thorin {

class Sym {
public:
    Sym() = default;
    Sym(const std::string* ptr)
        : ptr_(ptr) {}

    bool is_anonymous() const { return ptr_ && ptr_->size() == 1 && ptr_->front() == '_'; }
    bool operator==(Sym other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym other) const { return this->ptr_ == other.ptr_; }
    operator const std::string_view() const { return *ptr_; }
    operator const std::string&() const { return *ptr_; }
    explicit operator bool() const { return ptr_; }
    const std::string& operator*() const { return *ptr_; }
    const std::string* operator->() const { return ptr_; }

private:
    const std::string* ptr_ = nullptr;

    template<class H>
    friend H AbslHashValue(H h, Sym sym) {
        return H::combine(std::move(h), sym.ptr_);
    }
};

inline std::ostream& operator<<(std::ostream& o, const Sym sym) { return o << *sym; }

class Pool {
public:
    Pool()            = default;
    Pool(const Pool&) = delete;
    Pool(Pool&& other)
        : pool_(std::move(other.pool_)) {}

    Sym sym(std::string_view s) { return &*pool_.emplace(s).first; }
    Sym sym(const char* s) { return &*pool_.emplace(s).first; }
    Sym sym(std::string&& s) { return &*pool_.emplace(std::move(s)).first; }

    friend void swap(Pool& p1, Pool& p2) {
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

} // namespace thorin
