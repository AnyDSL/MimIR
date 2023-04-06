#pragma once

#include <array>
#include <istream>
#include <optional>

#include "thorin/util/loc.h"
#include "thorin/util/types.h"

namespace thorin::utf8 {

static constexpr size_t Max   = 4;          ///< Maximal number of `char8_t`s of an UTF-8 byte sequence.
static constexpr char32_t BOM = 0xfeff_u32; ///< [Byte Order Mark](https://en.wikipedia.org/wiki/Byte_order_mark#UTF-8).
static constexpr char32_t EoF = (char32_t)std::istream::traits_type::eof(); ///< End of File.
static constexpr char32_t Err = (char32_t)-2;                               ///< Erroneous UTF char.

/// Returns the expected number of bytes for an UTF-8 char sequence by inspecting the first byte.
/// Retuns @c 0 if invalid.
size_t num_bytes(char8_t c);

/// Append @p b to @p c for converting UTF-8 to UTF-32.
inline char32_t append(char32_t c, char32_t b) { return (c << 6_u32) | (b & 0b00111111_u32); }

/// Get relevant bits of first UTF-8 byte @p c of a @em multi-byte sequence consisting of @p num bytes.
inline char32_t first(char32_t c, char32_t num) { return c & (0b00011111_u32 >> (num - 2_u32)); }

/// Is the 2nd, 3rd, or 4th byte of an UTF-8 byte sequence valid?
inline std::optional<char8_t> is_valid(char8_t c) {
    return (c & 0b11000000_u8) == 0b10000000_u8 ? (c & 0b00111111_u8) : std::optional<char8_t>();
}

/// Encodes the next sequence of bytes from @p is as UTF-32.
/// @returns `std::nullopt` on error.
char32_t encode(std::istream& is);

/// Decodes the UTF-32 char @p c to UTF-8 and writes the sequence of bytes to @p os.
/// @returns `false` on error.
bool decode(std::ostream& os, char32_t c);

template<size_t Max_Ahead>
class Lexer {
public:
    Lexer(std::istream& istream, const fs::path* path)
        : istream_(istream)
        , loc_(path, {0, 0}) {
        ahead_.back().pos = {1, 0};
        for (size_t i = 0; i != Max_Ahead; ++i) next();
        accept(BOM); // eat utf-8 BOM if present
    }
    virtual ~Lexer() {}

protected:
    struct Char {
        Char() = default;
        Char(char32_t c32, Pos pos)
            : c32(c32)
            , pos(pos) {}

        operator char32_t() const { return c32; }
        char32_t c32;
        Pos pos;
    };

    Char ahead(size_t i = 0) const {
        assert(i < Max_Ahead);
        return ahead_[i];
    }

    virtual Char next() {
        auto result = ahead();
        auto prev   = ahead_.back().pos;

        for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead_[i] = ahead_[i + 1];
        auto& back = ahead_.back();
        back.pos   = prev;
        back.c32   = utf8::encode(istream_);

        if (back.c32 == '\n') {
            ++back.pos.row;
            back.pos.col = 0;
        } else if (back.c32 == EoF) {
            /* do nothing */
        } else {
            ++back.pos.col;
        }

        loc_.finis = result.pos;
        return result;
    }

    /// Yields `true` if @p pred holds.
    /// In this case invoke @p next() and append to @p str_;
    template<class Pred>
    bool accept_if(Pred pred, bool append = true) {
        if (pred(ahead())) {
            if (append) str_ += ahead();
            next();
            return true;
        }
        return false;
    }

    bool accept(char32_t val, bool append = true) {
        return accept_if([val](char32_t p) { return p == val; }, append);
    }

    std::istream& istream_;
    Loc loc_;
    std::string str_;
    std::array<Char, Max_Ahead> ahead_;
};

} // namespace thorin::utf8
