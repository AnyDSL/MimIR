#pragma once

#include <optional>

#include <absl/container/flat_hash_map.h>
#include <fe/lexer.h>

#include "thorin/ast/tok.h"

namespace thorin::ast {

namespace fs = std::filesystem;

class AST;

class Lexer : public fe::Lexer<3, Lexer> {
    using Super = fe::Lexer<3, Lexer>;

public:
    /// Creates a lexer to read Thorin files (see [Lexical Structure](@ref lex)).
    /// If @p md is not `nullptr`, a Markdown output will be generated.
    Lexer(AST&, std::istream&, const fs::path* path = nullptr, std::ostream* md = nullptr);

    AST& ast() { return ast_; }
    const fs::path* path() const { return loc_.path; }
    Loc loc() const { return loc_; }
    Tok lex();

private:
    char32_t next() {
        auto res = Super::next();
        if (md_ && out_) {
            if (res == fe::utf8::EoF) {
                *md_ << "\n```\n";
                out_ = false;
            } else if (res) {
                bool success = fe::utf8::encode(*md_, res);
                assert_unused(success);
            }
        }
        return res;
    }

    Tok tok(Tok::Tag tag) { return {loc(), tag}; }
    Sym sym();
    Loc cache_trailing_dot();
    bool lex_id();
    char8_t lex_char();
    std::optional<Tok> parse_lit();
    void parse_digits(int base = 10);
    bool parse_exp(int base = 10);
    void eat_comments();
    bool start_md() const { return ahead(0) == '/' && ahead(1) == '/' && ahead(2) == '/'; }
    void emit_md(bool start_of_file = false);
    void md_fence() {
        if (md_) *md_ << "```\n";
    }

    AST& ast_;
    std::ostream* md_;
    bool out_ = true;
    fe::SymMap<Tok::Tag> keywords_;
    std::optional<Tok> cache_ = std::nullopt;

    friend class fe::Lexer<3, Lexer>;
};

} // namespace thorin::ast
