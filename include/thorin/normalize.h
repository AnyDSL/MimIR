#pragma once

#include "thorin/def.h"

namespace thorin {

class Res {
public:
    Res()
        : data_{} {}
    template<class T>
    Res(T val)
        : data_(thorin::bitcast<u64>(val)) {}

    constexpr const u64& operator*() const& { return *data_; }
    constexpr u64& operator*() & { return *data_; }
    explicit operator bool() const { return data_.has_value(); }

private:
    std::optional<u64> data_;
};

// Swap Lit to left - or smaller Def::gid, if no lit present.
inline void commute(bool commutative, const Def*& a, const Def*& b) {
    if (commutative) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>())) std::swap(a, b);
    }
}

} // namespace thorin
