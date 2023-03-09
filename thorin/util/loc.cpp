#include "thorin/util/loc.h"

namespace thorin {

std::ostream& operator<<(std::ostream& os, const Pos pos) {
    if (pos.row) {
        if (pos.col) return os << pos.row << ':' << pos.col;
        return os << pos.row;
    }
    return os << "<unknown position>";
}

std::ostream& operator<<(std::ostream& os, const Loc loc) {
    if (loc.begin) {
        os << loc.file << ':' << loc.begin;
        if (loc.begin != loc.finis) os << '-' << loc.finis;
        return os;
    }
    return os << "<unknown location>";
}

void Pos::dump() { std::cout << *this << std::endl; }
void Loc::dump() { std::cout << *this << std::endl; }

} // namespace thorin
