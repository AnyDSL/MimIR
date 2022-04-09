#ifndef THORIN_CLI_MD_CONVERTER_H
#define THORIN_CLI_MD_CONVERTER_H

#include <cassert>

#include "thorin/util/utf8.h"

namespace thorin {

#if 0
class MDConverter : public utf8::Reader {
public:
    MDConverter(std::istream&, std::ostream&);
    void run();

private:
    char32_t next();
    void next(size_t n);
    char32_t ahead(size_t i = 0) const {
        assert(i < Max_Ahead);
        return ahead_[i];
    }
    void eat_whitespace();
    bool eof() const { return ahead() == (char32_t)std::istream::traits_type::eof(); }

    enum class State { Neither, Code, Text } state_ = State::Neither;

    static constexpr size_t Max_Ahead = 2; ///< maximum lookahead
    std::array<char32_t, Max_Ahead> ahead_;
    std::ostream& ostream_;
};
#endif

} // namespace thorin

#endif
