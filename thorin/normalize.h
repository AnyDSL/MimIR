#pragma once

#include <optional>

#include "thorin/axiom.h"

#include "thorin/util/cast.h"
#include "thorin/util/types.h"

namespace thorin::normalize {

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

template<class T>
static T get(u64 u) {
    return thorin::bitcast<T>(u);
}

template<class Id> constexpr bool is_commutative(Id) { return false; }

/// @warning By default we assume that any commutative operation is also associative.
/// Please provide a proper specialization if this is not the case.
template<class Id>
constexpr bool is_associative(Id id) { return is_commutative(id); }

template<class Id>
static void commute(Id id, const Def*& a, const Def*& b) {
    if (is_commutative(id)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>()))
            std::swap(a, b); // swap lit to left, or smaller gid to left if no lit present
    }
}

} // namespace thorin::normalize
