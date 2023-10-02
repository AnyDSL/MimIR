#include "thorin/util/utf8.h"

#include <fe/assert.h>

namespace thorin::utf8 {

namespace {
// and, or
std::ostream& ao(std::ostream& os, char32_t c32, char32_t a = 0b00111111_u32, char32_t o = 0b10000000_u32) {
    return os << char((c32 & a) | o);
}
} // namespace

size_t num_bytes(char8_t c) {
    if ((c & 0b10000000_u8) == 0b00000000_u8) return 1;
    if ((c & 0b11100000_u8) == 0b11000000_u8) return 2;
    if ((c & 0b11110000_u8) == 0b11100000_u8) return 3;
    if ((c & 0b11111000_u8) == 0b11110000_u8) return 4;
    return 0;
}

char32_t encode(std::istream& is) {
    char32_t result = is.get();
    if (result == EoF) return result;

    switch (auto n = utf8::num_bytes(result)) {
        case 0: return {};
        case 1: return result;
        default:
            result = utf8::first(result, n);

            for (size_t i = 1; i != n; ++i)
                if (auto x = utf8::is_valid(is.get()))
                    result = utf8::append(result, *x);
                else
                    return Err;
    }

    return result;
}

bool decode(std::ostream& os, char32_t c32) {
    // clang-format off
    if (c32 <= 0x00007f_u32) {          ao(os, c32      , 0b11111111_u32, 0b00000000_u32);                              return true; }
    if (c32 <= 0x0007ff_u32) {       ao(ao(os, c32 >>  6, 0b00011111_u32, 0b11000000_u32),                        c32); return true; }
    if (c32 <= 0x00ffff_u32) {    ao(ao(ao(os, c32 >> 12, 0b00001111_u32, 0b11100000_u32),             c32 >> 6), c32); return true; }
    if (c32 <= 0x10ffff_u32) { ao(ao(ao(ao(os, c32 >> 18, 0b00000111_u32, 0b11110000_u32), c32 >> 12), c32 >> 6), c32); return true; }
    // clang-format on
    return false;
}

} // namespace thorin::utf8
