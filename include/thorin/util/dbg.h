#pragma once

#include <filesystem>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fe/loc.h>
#include <fe/sym.h>
#include <rang.hpp>

#include "thorin/util/print.h"

namespace thorin {

namespace fs = std::filesystem;

using fe::Loc;
using fe::Pos;
using fe::Sym;

/// @name Formatted Output
///@{
/// Prefixes error message with `<location>: error: `.
template<class T = std::logic_error, class... Args> [[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream o;
    print(o, "{}{}: {}error: {}", rang::fg::yellow, loc, rang::fg::red, rang::fg::reset);
    print(o, fmt, std::forward<Args&&>(args)...);
    throw T(o.str());
}
///@}

struct Dbg {
    constexpr Dbg() noexcept           = default;
    constexpr Dbg(const Dbg&) noexcept = default;
    constexpr Dbg(Loc loc, Sym sym) noexcept
        : loc(loc)
        , sym(sym) {}
    constexpr Dbg(Loc loc) noexcept
        : Dbg(loc, {}) {}
    Dbg& operator=(const Dbg&) noexcept = default;

    Loc loc;
    Sym sym;

    explicit operator bool() const { return sym.operator bool(); }

    friend std::ostream& operator<<(std::ostream& os, const Dbg& dbg) { return os << dbg.sym; }
};

} // namespace thorin
