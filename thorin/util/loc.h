#pragma once

#include <filesystem>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "thorin/util/print.h"
#include "thorin/util/sym.h"

namespace thorin {

namespace fs = std::filesystem;

struct Pos {
    Pos() = default;
    Pos(uint16_t row)
        : row(row) {}
    Pos(uint16_t row, uint16_t col)
        : row(row)
        , col(col) {}

    explicit operator bool() const { return row; }
    bool operator==(Pos other) const { return row == other.row && col == other.col; }
    void dump();

    uint16_t row = 0;
    uint16_t col = 0;
};

/// Loc%ation in a File.
/// @warning Loc::path is only a pointer and it is your job to guarantee
/// that the underlying `std::filesystem::path` outlives this Loc%ation.
/// In Thorin itself, the Driver takes care of this.
struct Loc {
    Loc() = default;
    Loc(const fs::path* path, Pos begin, Pos finis)
        : path(path)
        , begin(begin)
        , finis(finis) {}
    Loc(const fs::path* file, Pos pos)
        : Loc(file, pos, pos) {}
    Loc(Pos begin, Pos finis)
        : Loc(nullptr, begin, finis) {}
    Loc(Pos pos)
        : Loc(nullptr, pos, pos) {}

    Loc anew_begin() const { return {path, begin, begin}; }
    Loc anew_finis() const { return {path, finis, finis}; }
    explicit operator bool() const { return (bool)begin; }
    /// @note Loc::path is only checked via pointer equality.
    bool operator==(Loc other) const { return begin == other.begin && finis == other.finis && path == other.path; }
    void dump();

    const fs::path* path = nullptr;
    Pos begin            = {};
    Pos finis            = {};
    ///< It's called `finis` because it refers to the **last** character within this Loc%ation.
    /// In the STL the word `end` refers to the position of something that is one element **past** the end.
};

/// @name Formatted Output
///@{
/// Prefixes error message with `<location>: error: `.
template<class T = std::logic_error, class... Args>
[[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream o;
    print(o, "{}: error: ", loc);
    print(o, fmt, std::forward<Args&&>(args)...);
    throw T(o.str());
}
///@}

/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream&, const Pos);
std::ostream& operator<<(std::ostream&, const Loc);
///@}

struct Dbg {
    Loc loc;
    Sym sym;
};

} // namespace thorin
