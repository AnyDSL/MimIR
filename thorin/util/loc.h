#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "thorin/util/sym.h"
#include "thorin/util/print.h"

namespace thorin {

struct Pos {
    Pos() = default;
    Pos(uint16_t row)
        : row(row) {}
    Pos(uint16_t row, uint16_t col)
        : row(row)
        , col(col) {}

    explicit operator bool() const { return row; }

    uint16_t row = 0;
    uint16_t col = 0;
};

struct Loc {
    Loc() = default;
    Loc(Sym file, Pos begin, Pos finis)
        : file(file)
        , begin(begin)
        , finis(finis) {}
    Loc(Sym file, Pos pos)
        : Loc(file, pos, pos) {}

    Loc anew_begin() const { return {file, begin, begin}; }
    Loc anew_finis() const { return {file, finis, finis}; }
    explicit operator bool() const { return (bool)begin; }

    Sym file;
    Pos begin = {};
    Pos finis = {};
    ///< It's called `finis` because it refers to the **last** character within this Loc%ation.
    /// In the STL the word `end` refers to the position of something that is one element **past** the end.
};

template<class T = std::logic_error, class... Args>
[[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream o;
    print(o, "{}: error: ", loc);
    print(o, fmt, std::forward<Args&&>(args)...);
    throw T(o.str());
}

std::ostream& operator<<(std::ostream&, const Pos);
std::ostream& operator<<(std::ostream&, const Loc);

inline bool operator==(Pos p1, Pos p2) { return p1.row == p2.row && p1.col == p2.col; }
inline bool operator==(Loc l1, Loc l2) { return l1.begin == l2.begin && l1.finis == l2.finis && l1.file == l2.file; }

struct Dbg {
    Loc loc;
    Sym sym;
};

} // namespace thorin
