#include "thorin/debug.h"

#include "thorin/world.h"

namespace thorin {

Loc::Loc(const Def* dbg) {
    if (dbg != nullptr) {
        auto [d_file, d_begin, d_finis] = dbg->proj(1)->projs<3>();

        file      = tuple2str(d_file);
        begin.row = u32(as_lit(d_begin) >> 32_u64);
        begin.col = u32(as_lit(d_begin));
        finis.row = u32(as_lit(d_finis) >> 32_u64);
        finis.col = u32(as_lit(d_finis));
    }
}

Debug::Debug(const Def* dbg)
    : name(dbg ? tuple2str(dbg->proj(0)) : std::string{})
    , loc(dbg)
    , meta(dbg ? dbg->proj(2) : nullptr) {}

size_t SymHash::operator()(Sym sym) const { return murmur3(sym.def()->gid()); }

Loc Sym::loc() const { return def()->loc(); }

/*
 * ostream
 */

std::ostream& operator<<(std::ostream& os, const Pos pos) { return os << pos.row << ':' << pos.col; }
std::ostream& operator<<(std::ostream& os, const Sym sym) { return os << tuple2str(sym.def()); }

std::ostream& operator<<(std::ostream& os, const Loc loc) {
    os << loc.file << ':';

    if (loc.begin.col == u32(-1) || loc.finis.col == u32(-1)) {
        if (loc.begin.row != loc.finis.row)
            return os << loc.begin.row << '_' << loc.finis.row;
        else
            return os << loc.begin.row;
    } else if (loc.begin.row != loc.finis.row) {
        return os << loc.begin << '-' << loc.finis;
    } else if (loc.begin.col != loc.finis.col) {
        return os << loc.begin << '-' << loc.finis.col;
    } else {
        return os << loc.begin;
    }
}

std::string Sym::to_string() const { return tuple2str(def()); }

} // namespace thorin
