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
char ao(char32_t c32, char32_t a = char32_t(0b00111111), char32_t o = char32_t(0b10000000)) {
    return char((c32 & a) | o);
}

std::array<char, 5> decode(char32_t c32) {
    // clang-format off
    if (c32 <= 0x00007f) return {char(c32), 0, 0, 0, 0};
    if (c32 <= 0x0007ff) return {ao(c32 >>  6, 0b11111, 0b11000000), ao(c32      ),            0,       0, 0};
    if (c32 <= 0x00ffff) return {ao(c32 >> 12, 0b01111, 0b11100000), ao(c32 >>  6), ao(c32     ),       0, 0};
    if (c32 <= 0x10ffff) return {ao(c32 >> 18, 0b00111, 0b11110000), ao(c32 >> 12), ao(c32 >> 6), ao(c32), 0};
    // clang-format on
    unreachable();
}

std::optional<char32_t> Reader::encode() {
    char32_t result  = istream_.get();
    if (result == (char32_t)std::istream::traits_type::eof()) return result;

    switch (auto n = utf8::num_bytes(result)) {
        case 0: return {};
        case 1: return result;
        default:
            result = utf8::first(result, n);

            for (size_t i = 1; i != n; ++i) {
                if (auto x = utf8::is_valid(istream_.get()))
                    result = utf8::append(result, *x);
                else
                    return {};
            }
    }

    return result;
}

}
