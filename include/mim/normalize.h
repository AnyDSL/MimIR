#pragma once

#include "mim/def.h"

namespace mim {

/// Utility class when folding constants in normalizers.
class Res {
public:
    Res()
        : data_{} {}
    template<class T>
    Res(T val)
        : data_(mim::bitcast<u64>(val)) {}

    constexpr const u64& operator*() const& { return *data_; }
    constexpr u64& operator*() & { return *data_; }
    explicit operator bool() const { return data_.has_value(); }

private:
    std::optional<u64> data_;
};

} // namespace mim
