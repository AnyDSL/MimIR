#pragma once

#include <filesystem>
#include <stdexcept>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fe/loc.h>
#include <fe/sym.h>

#include "thorin/util/print.h"

namespace thorin {

namespace fs = std::filesystem;

using fe::Loc;
using fe::Pos;
using fe::Sym;

struct Error : std::logic_error {
    Error(Loc loc, const std::string& what)
        : std::logic_error(what)
        , loc(loc) {}

    Loc loc;
};

/// @name Formatted Output
///@{
/// Prefixes error message with `<location>: error: `.
template<class... Args> [[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream o;
    print(o, fmt, std::forward<Args&&>(args)...);
    throw Error(loc, o.str());
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
