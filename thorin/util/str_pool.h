#pragma once

#include <string>
#include <absl/container/node_hash_set.h>

namespace thorin {

class Sym_ {
public:
    Sym_() = default;
    Sym_(const std::string* ptr)
        : ptr_(ptr) {}

    bool operator==(Sym_ other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym_ other) const { return this->ptr_ == other.ptr_; }
    operator const std::string_view() const { return *ptr_; }
    operator const std::string&() const { return *ptr_; }
    const std::string& operator*() const { return *ptr_; }
    const std::string& operator->() const { return *ptr_; }

private:
    const std::string* ptr_ = nullptr;

    template<class H>
    friend H AbslHashValue(H h, Sym_ sym) {
        return H::combine(std::move(h), sym.ptr_);
    }
};

class Syms {
public:
    Syms() = default;
    Syms(const Syms&) = delete;
    Syms(Syms&& other)
        : pool_(std::move(other.pool_)) {}

    Sym_ sym(std::string_view s) { return &*pool_.emplace(s).first; }
    Sym_ sym(const char* s) { return &*pool_.emplace(s).first; }
    Sym_ sym(std::string&& s) { return &*pool_.emplace(std::move(s)).first; }

    friend void swap(Syms& p1, Syms& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

}
