#ifndef THORIN_DEBUG_H
#define THORIN_DEBUG_H

#include <string>
#include <tuple>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "thorin/util/stream.h"

namespace thorin {

class Def;

struct Pos : public Streamable<Pos> {
    Pos() = default;
    Pos(uint32_t row, uint32_t col)
        : row(row)
        , col(col) {}

    Stream& stream(Stream&) const;

    uint32_t row = -1;
    uint32_t col = -1;
};

struct Loc : public Streamable<Loc> {
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

    std::string file;
    Pos begin = {uint32_t(-1), uint32_t(-1)};
    Pos finis = {uint32_t(-1), uint32_t(-1)};
    ///< It's called `finis` because it refers to the *last* character within this @p Loc%ation.
    /// In the STL the word `end` refers to the position of something that is one element *past* the end.

    Stream& stream(Stream&) const;
};

inline bool operator==(Pos p1, Pos p2) { return p1.row == p2.row && p1.col == p2.col; }
inline bool operator==(Loc l1, Loc l2) { return l1.begin == l2.begin && l1.finis == l2.finis && l1.file == l2.file; }

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
    Debug(Loc loc)
        : Debug(std::string(), loc) {}
    Debug(const Def*);

    std::string name;
    Loc loc;
    const Def* meta = nullptr;
};

class Sym : public Streamable<Sym> {
public:
    Sym() {}
    Sym(const Def* def)
        : def_(def) {}

    const Def* def() const { return def_; }
    Debug debug();
    std::string to_string() const;
    const Def* operator->() const { return def_; }
    operator bool() const { return def_; }
    operator std::string() const { return to_string(); }
    bool operator==(Sym other) const { return this->def() == other.def(); }
    Stream& stream(Stream& s) const;

private:
    const Def* def_ = nullptr;
};

struct SymHash {
    size_t operator()(Sym) const;
};

struct SymEq {
    bool operator()(Sym s1, Sym s2) const { return s1 == s2; }
};

template<class Val>
using SymMap = absl::flat_hash_map<Sym, Val, SymHash, SymEq>;
using SymSet = absl::flat_hash_set<Sym, SymHash, SymEq>;

} // namespace thorin

#endif
