#include "thorin/util/print.h"

namespace thorin {

std::ostream& print(std::ostream& os, const char* s) {
    while (*s) {
        auto next = s + 1;

        switch (*s) {
            case '{':
                if (detail::match2nd(os, next, s, '{')) continue;
                while (*s && *s != '}') s++;
                assert(*s != '}' && "invalid format string for 'streamf': missing argument(s)");
                unreachable();
                break;
            case '}':
                if (detail::match2nd(os, next, s, '}')) continue;
                assert(false && "unmatched/unescaped closing brace '}' in format string");
                unreachable();
            default: os << *s++;
        }
    }
    return os;
}

namespace detail {

bool match2nd(std::ostream& os, const char* next, const char*& s, const char c) {
    if (*next == c) {
        os << c;
        s += 2;
        return true;
    }
    return false;
}

} // namespace detail

} // namespace thorin
