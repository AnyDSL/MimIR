#include "thorin/util/dbg.h"

namespace fe {

std::ostream& operator<<(std::ostream& os, const Pos pos) {
    if (pos.row) {
        if (pos.col) return os << pos.row << ':' << pos.col;
        return os << pos.row;
    }
    return os << "<unknown position>";
}

std::ostream& operator<<(std::ostream& os, const Loc loc) {
    if (loc) {
        os << (loc.path ? *loc.path : "<unknown file>") << ':' << loc.begin;
        if (loc.begin != loc.finis) {
            if (loc.begin.row != loc.finis.row)
                os << '-' << loc.finis;
            else
                os << '-' << loc.finis.col;
        }
        return os;
    }
    return os << "<unknown location>";
}

} // namespace fe
