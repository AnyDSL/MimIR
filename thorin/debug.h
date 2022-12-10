#pragma once

#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "thorin/util/print.h"

namespace thorin {

class Def;
class World;

struct Pos {
    Pos() = default;
    Pos(uint32_t row, uint32_t col)
        : row(row)
        , col(col) {}
    Pos(uint64_t rowcol)
        : row(rowcol >> uint64_t(32))
        , col(uint32_t(rowcol)) {}
    Pos(const Def*);

    uint64_t rowcol() const { return (uint64_t(row) << uint64_t(32)) | uint64_t(col); }
    const Def* def(World&) const;
    explicit operator bool() const { return row != uint32_t(-1); }

    uint32_t row = -1;
    uint32_t col = -1;
};

struct Loc {
    Loc() = default;
    Loc(std::string_view file, Pos begin, Pos finis)
        : file(file)
        , begin(begin)
        , finis(finis) {}
    Loc(std::string_view file, Pos pos)
        : Loc(file, pos, pos) {}
    Loc(const Def* dbg);

    Loc anew_begin() const { return {file, begin, begin}; }
    Loc anew_finis() const { return {file, finis, finis}; }
    const Def* def(World&) const;
    explicit operator bool() const { return (bool)begin; }

    std::string file;
    Pos begin = {uint32_t(-1), uint32_t(-1)};
    Pos finis = {uint32_t(-1), uint32_t(-1)};
    ///< It's called `finis` because it refers to the **last** character within this Loc%ation.
    /// In the STL the word `end` refers to the position of something that is one element **past** the end.
};

template<class T = std::logic_error, class... Args>
[[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream oss;
    print(oss, "{}: error: ", loc);
    print(oss, fmt, std::forward<Args&&>(args)...);
    throw T(oss.str());
}

class Sym {
public:
    Sym() {}
    Sym(const Def* str, const Def* loc)
        : str_(str)
        , loc_(loc) {}

    const Def* str() const { return str_; }
    const Def* loc() const { return loc_; }

    std::string to_string() const;
    Loc to_loc() const { return loc_ ? loc_ : Loc(); }
    bool is_anonymous() const;

    operator std::string() const { return to_string(); }
    operator Loc() const { return loc_; }
    operator bool() const { return str_; }

private:
    const Def* str_ = nullptr;
    const Def* loc_ = nullptr;
};

class Debug {
public:
    Debug(std::string_view name, Loc loc = {}, const Def* meta = nullptr)
        : name(name)
        , loc(loc)
        , meta(meta) {}
    Debug(std::string name, Loc loc = {}, const Def* meta = nullptr)
        : name(name)
        , loc(loc)
        , meta(meta) {}
    Debug(const char* name, Loc loc = {}, const Def* meta = nullptr)
        : Debug(std::string(name), loc, meta) {}
    Debug(Sym sym, const Def* meta = nullptr)
        : Debug(sym.to_string(), sym.to_loc(), meta) {}
    Debug(Sym sym, Loc loc, const Def* meta = nullptr)
        : Debug(sym.to_string(), loc, meta) {}
    Debug(Loc loc)
        : Debug(std::string(), loc) {}
    Debug(const Def*);

    const Def* def(World&) const;

    std::string name;
    Loc loc;
    const Def* meta = nullptr;
};

std::ostream& operator<<(std::ostream&, const Pos);
std::ostream& operator<<(std::ostream&, const Loc);
std::ostream& operator<<(std::ostream&, const Sym);

inline bool operator==(Sym s1, Sym s2) { return s1.str() == s2.str(); } // don't cmp loc
inline bool operator==(Pos p1, Pos p2) { return p1.row == p2.row && p1.col == p2.col; }
inline bool operator==(Loc l1, Loc l2) { return l1.begin == l2.begin && l1.finis == l2.finis && l1.file == l2.file; }

struct SymHash {
    size_t operator()(Sym) const;
};

template<class Val>
using SymMap = absl::flat_hash_map<Sym, Val, SymHash>;
using SymSet = absl::flat_hash_set<Sym, SymHash>;

} // namespace thorin
