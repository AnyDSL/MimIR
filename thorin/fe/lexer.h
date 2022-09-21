#pragma once

#include <absl/container/flat_hash_map.h>

#include "thorin/debug.h"
#include "thorin/error.h"

#include "thorin/fe/tok.h"
#include "thorin/util/utf8.h"

namespace thorin {

class World;

namespace fe {

class Lexer : public utf8::Lexer<3> {
    using Super = utf8::Lexer<3>;

public:
    /// Creates a lexer to read Thorin files (see [Lexical Structure](@ref lex)).
    /// If @p md is not `nullptr`, a Markdown output will be generated.
    Lexer(World& world, std::string_view file, std::istream& istream, std::ostream* md = nullptr);

    World& world() { return world_; }
    std::string_view file() const { return loc_.file; }
    Loc loc() const { return loc_; }
    Tok lex();

private:
    Ahead next() override {
        auto res = Super::next();
        if (md_ && out_) {
            if (res.c32 == utf8::EoF) {
                *md_ << "\n```\n";
                out_ = false;
            } else if (res.c32 != utf8::Err) {
                bool success = utf8::decode(*md_, res.c32);
                assert_unused(success);
            }
        }
        return res;
    }

    Tok tok(Tok::Tag tag) { return {loc(), tag}; }
    bool lex_id();
    std::optional<Tok> parse_lit();
    void parse_digits(int base = 10);
    bool parse_exp(int base = 10);
    void eat_comments();
    bool start_md() const { return ahead(0).c32 == '/' && ahead(1).c32 == '/' && ahead(2).c32 == '/'; }
    void emit_md(bool start_of_file = false);
    void md_fence() {
        if (md_) *md_ << "```\n";
    }

    World& world_;
    std::ostream* md_;
    bool out_ = true;
    absl::flat_hash_map<std::string, Tok::Tag> keywords_;
};

} // namespace fe
} // namespace thorin
