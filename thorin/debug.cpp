#include "thorin/debug.h"

#include "thorin/world.h"

namespace thorin {

/*
 * c'tor
 */

Pos::Pos(const Def* def)
    : Pos(as_lit(def)) {}

Loc::Loc(const Def* def)
    : file(tuple2str(def->proj(3, 0_s)))
    , begin(def->proj(3, 1_s))
    , finis(def->proj(3, 2_s)) {}

Debug::Debug(const Def* def)
    : name(def ? tuple2str(def->proj(3, 0_s)) : std::string())
    , loc(def ? def->proj(3, 1_s) : Loc())
    , meta(def ? def->proj(3, 2_s) : nullptr) {}

/*
 * conversion
 */

const Def* Pos::def(World& w) const { return w.lit_nat(rowcol()); }

const Def* Loc::def(World& w) const {
    auto d_file  = w.tuple_str(file);
    auto d_begin = begin.def(w);
    auto d_finis = finis.def(w);
    return w.tuple({d_file, d_begin, d_finis});
}

const Def* Debug::def(World& w) const {
    auto d_name = w.tuple_str(name);
    auto d_loc  = loc.def(w);
    auto d_meta = meta ? meta : w.bot(w.type_bot());

    return w.tuple({d_name, d_loc, d_meta});
}

/*
 * Sym
 */

size_t SymHash::operator()(Sym sym) const { return murmur3(sym.str()->gid()); }
std::string Sym::to_string() const { return tuple2str(str()); }

bool Sym::is_anonymous() const {
    if (auto lit = isa_lit(str())) return lit == '_';
    return false;
}

/*
 * ostream
 */

std::ostream& operator<<(std::ostream& os, const Pos pos) {
    if (pos.col != uint32_t(-1)) return os << pos.row << ':' << pos.col;
    return os << pos.row;
}

std::ostream& operator<<(std::ostream& os, const Loc loc) {
    if (!loc.begin) return os << "<unknown location>";
    os << loc.file << ':' << loc.begin;
    if (loc.begin != loc.finis) os << '-' << loc.finis;
    return os;
}

std::ostream& operator<<(std::ostream& os, const Sym sym) { return os << sym.to_string(); }

} // namespace thorin
