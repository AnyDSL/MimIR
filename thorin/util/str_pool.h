#pragma once

#include <string>
#include <absl/container/node_hash_set.h>

namespace thorin {

class Sym_ {
public:
    Sym_(const std::string* ptr)
        : ptr_(ptr) {}

    bool operator==(Sym_ other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym_ other) const { return this->ptr_ == other.ptr_; }
    operator const std::string_view() const { return *ptr_; }
    operator const std::string&() const { return *ptr_; }
    const std::string& operator*() const { return *ptr_; }
    const std::string& operator->() const { return *ptr_; }

private:
    const std::string* ptr_;
};

class StrPool {
public:
    StrPool() = default;
    StrPool(const StrPool&) = delete;
    StrPool(StrPool&& other)
        : pool_(std::move(other.pool_)) {}

    Sym_ add(std::string_view s) {
        auto [i, _] = pool_.emplace(s);
        return Sym_(&*i);
    }

    friend void swap(StrPool& p1, StrPool& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

}
