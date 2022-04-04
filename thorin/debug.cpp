#include "thorin/debug.h"

#include "thorin/world.h"

namespace thorin {

Loc::Loc(const Def* dbg) {
    if (dbg != nullptr) {
        auto [d_file, d_begin, d_finis] = dbg->proj(1)->projs<3>();
        file                            = tuple2str(d_file);
        begin.row                       = u32(as_lit(d_begin) >> 32_u64);
        begin.col                       = u32(as_lit(d_begin));
        finis.row                       = u32(as_lit(d_finis) >> 32_u64);
        finis.col                       = u32(as_lit(d_finis));
    }
}

Debug::Debug(const Def* dbg)
    : name(dbg ? tuple2str(dbg->proj(0)) : std::string{})
    , loc(dbg)
    , meta(dbg ? dbg->proj(2) : nullptr) {}

size_t SymHash::operator()(Sym sym) const { return murmur3(sym.def()->gid()); }

Stream& Pos::stream(Stream& s) const { return s.fmt("{}:{}", row, col); }

Stream& Loc::stream(Stream& s) const {
    s.fmt("{}:", file);

    if (begin.col == u32(-1) || finis.col == u32(-1)) {
        if (begin.row != finis.row)
            s.fmt("{}-{}", begin.row, finis.row);
        else
            s.fmt("{}", begin.row);
    } else if (begin.row != finis.row) {
        s.fmt("{}-{}", begin, finis);
    } else if (begin.col != finis.col) {
        s.fmt("{}-{}", begin, finis.col);
    } else {
        begin.stream(s);
    }

    return s;
}

std::string Sym::to_string() const { return tuple2str(def()); }
Stream& Sym::stream(Stream& s) const { return s << tuple2str(def()); }

} // namespace thorin
