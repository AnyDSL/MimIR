#include "cli/md_converter.h"

namespace thorin {

MDConverter::MDConverter(std::istream& istream, std::ostream& ostream)
    : utf8::Reader(istream)
    , ostream_(ostream) {
    for (size_t i = 0; i != Max_Ahead; ++i) next();
}

char32_t MDConverter::next() {
    auto result = ahead();
    for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead_[i] = ahead_[i + 1];

    std::optional<char32_t> c32;
    while (!c32) c32 = encode();
    ahead_.back() = *c32;

    return result;
}

void MDConverter::next(size_t n) {
    for (size_t i = 0; i != n; ++i) next();
}

void MDConverter::eat_whitespace() {
    if (isspace(ahead())) next();
}

void MDConverter::run() {
    while (true) {
        if (eof()) return;
        if (ahead(0) == '/' && ahead(1) == '*') {
            next(2);
            eat_whitespace();

            while (ahead(0) != '*' || ahead(1) != '/') utf8::decode(ostream_, next());
        } else {
            next();
        }
    }
}

} // namespace thorin
