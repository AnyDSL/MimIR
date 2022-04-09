#ifndef THORIN_FE_LEXER_H
#define THORIN_FE_LEXER_H

#include <absl/container/flat_hash_map.h>

#include "thorin/debug.h"
#include "thorin/error.h"

#include "thorin/fe/tok.h"
#include "thorin/util/utf8.h"

namespace thorin {

class World;

class Lexer : public utf8::Lexer<3> {
    using Super = utf8::Lexer<3>;

public:
    /// Creates a lexer to read Thorin files (see #lex).
    /// If @p ostream is not `nullptr`, a Markdown output will be generated.
    Lexer(World& world, std::string_view file, std::istream& istream, std::ostream* ostream = nullptr);

    World& world() { return world_; }
    Loc loc() const { return loc_; }
    Tok lex();

private:
    Ahead next() override {
        auto res = Super::next();
        if (ostream_ && out_) {
            if (res.c32 == utf8::EoF) {
                *ostream_ << "\n```\n";
                out_ = false;
            } else if (res.c32 != utf8::Err) {
                utf8::decode(*ostream_, res.c32);
            }
        }
        return res;
    }

    template<class... Args>
    [[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
        thorin::err<LexError>(loc, fmt, std::forward<Args&&>(args)...);
    }

    Tok tok(Tok::Tag tag) { return {loc(), tag}; }
    bool lex_id();
    std::optional<Tok> parse_lit();
    void parse_digits(int base = 10);
    bool parse_exp(int base = 10);
    void eat_comments();
    bool start_md() const { return ahead(0).c32 == '/' && ahead(1).c32 == '/' && ahead(2).c32 == '/'; }
    void emit_md(bool start_of_file = false);
    void md_fence() { *ostream_ << "```\n"; }

    World& world_;
    std::ostream* ostream_;
    bool out_ = true;
    absl::flat_hash_map<std::string, Tok::Tag> keywords_;
};

} // namespace thorin

#endif
