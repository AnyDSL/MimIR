#include "thorin/util/utf8.h"

#include "thorin/util/assert.h"

namespace thorin::utf8 {

size_t num_bytes(char8_t c) {
    if ((c & 0b10000000_u8) == 0b00000000_u8) return 1;
    if ((c & 0b11100000_u8) == 0b11000000_u8) return 2;
    if ((c & 0b11110000_u8) == 0b11100000_u8) return 3;
    if ((c & 0b11111000_u8) == 0b11110000_u8) return 4;
    return 0;
}

// and, or
std::ostream& ao(std::ostream& os, char32_t c32, char32_t a = char32_t(0b00111111), char32_t o = char32_t(0b10000000)) {
    return os << char((c32 & a) | o);
}

std::ostream& decode(std::ostream& os, char32_t c32) {
    // clang-format off
    if (c32 <= 0x00007f) return          ao(os, c32      , 0b11111111, 0b00000000);
    if (c32 <= 0x0007ff) return       ao(ao(os, c32 >>  6, 0b00011111, 0b11000000),                        c32);
    if (c32 <= 0x00ffff) return    ao(ao(ao(os, c32 >> 12, 0b00001111, 0b11100000),             c32 >> 6), c32);
    if (c32 <= 0x10ffff) return ao(ao(ao(ao(os, c32 >> 18, 0b00000111, 0b11110000), c32 >> 12), c32 >> 6), c32);
    // clang-format on
    unreachable();
}

std::optional<char32_t> encode(std::istream& is) {
    char32_t result = is.get();
    if (result == (char32_t)std::istream::traits_type::eof()) return result;

    switch (auto n = utf8::num_bytes(result)) {
        case 0: return {};
        case 1: return result;
        default:
            result = utf8::first(result, n);

            for (size_t i = 1; i != n; ++i) {
                if (auto x = utf8::is_valid(is.get()))
                    result = utf8::append(result, *x);
                else
                    return {};
            }
    }

    return result;
}

} // namespace thorin::utf8
