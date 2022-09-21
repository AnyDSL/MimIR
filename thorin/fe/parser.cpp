#include "thorin/fe/parser.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/dialects.h"
#include "thorin/rewrite.h"

#include "thorin/util/array.h"
#include "thorin/util/sys.h"

// clang-format off
#define DECL                \
         Tok::Tag::K_ax:    \
    case Tok::Tag::K_cn:    \
    case Tok::Tag::K_let:   \
    case Tok::Tag::K_Sigma: \
    case Tok::Tag::K_Arr:   \
    case Tok::Tag::K_pack:  \
    case Tok::Tag::K_Pi:    \
    case Tok::Tag::K_lam:   \
    case Tok::Tag::K_def
// clang-format on

using namespace std::string_literals;

namespace thorin::fe {

Parser::Parser(World& world,
               std::string_view file,
               std::istream& istream,
               ArrayRef<std::string> import_search_paths,
               const Normalizers* normalizers,
               std::ostream* md)
    : lexer_(world, file, istream, md)
    , prev_(lexer_.loc())
    , bootstrapper_(std::filesystem::path{file}.filename().replace_extension("").string())
    , user_search_paths_(import_search_paths.begin(), import_search_paths.end())
    , normalizers_(normalizers) {
    for (size_t i = 0; i != Max_Ahead; ++i) lex();
    prev_ = Loc(file, {1, 1}, {1, 1});
}

Parser::Parser(World& world,
               std::string_view file,
               std::istream& istream,
               ArrayRef<std::string> import_search_paths,
               const Normalizers* normalizers,
               const Scopes& inhert_scopes,
               const SymSet& inhert_imported)
    : Parser(world, file, istream, import_search_paths, normalizers) {
    scopes_   = inhert_scopes;
    imported_ = inhert_imported;
}

/*
 * helpers
 */

Tok Parser::lex() {
    auto result = ahead();
    prev_       = ahead_[0].loc();
    for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead_[i] = ahead_[i + 1];
    ahead_.back() = lexer_.lex();
    return result;
}

std::optional<Tok> Parser::accept(Tok::Tag tag) {
    if (tag != ahead().tag()) return {};
    return lex();
}

Tok Parser::expect(Tok::Tag tag, std::string_view ctxt) {
    if (ahead().tag() == tag) return lex();

    std::string msg("'");
    msg.append(Tok::tag2str(tag)).append("'");
    syntax_err(msg, ctxt);
    return {};
}

void Parser::syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    err(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

/*
 * entry points
 */

Parser Parser::import_module(World& world,
                             std::string_view name,
                             ArrayRef<std::string> user_search_paths,
                             const Normalizers* normalizers) {
    auto search_paths = get_plugin_search_paths(user_search_paths);

    auto file_name = std::string(name) + ".thorin";

    std::string input_path{};
    for (const auto& path : search_paths) {
        auto full_path = path / file_name;

        std::error_code ignore;
        if (bool reg_file = std::filesystem::is_regular_file(full_path, ignore); reg_file && !ignore) {
            input_path = full_path.string();
            break;
        }
    }
    std::ifstream ifs(input_path);

    if (!ifs) throw std::runtime_error("could not find file '" + file_name + "'");

    thorin::fe::Parser parser(world, input_path, ifs, user_search_paths, normalizers);
    parser.parse_module();

    world.add_imported(name);

    return parser;
}

void Parser::bootstrap(std::ostream& h) { bootstrapper_.emit(h); }

void Parser::parse_module() {
    while (ahead().tag() == Tok::Tag::K_import) parse_import();

    parse_decls(false);
    expect(Tok::Tag::M_eof, "module");
};

/*
 * misc
 */

void Parser::parse_import() {
    eat(Tok::Tag::K_import);
    auto name = expect(Tok::Tag::M_id, "import name");
    expect(Tok::Tag::T_semicolon, "end of import");
    auto name_str = name.sym().to_string();

    if (auto it = imported_.find(name.sym()); it != imported_.end()) return;

    // search file and import
    auto parser = Parser::import_module(world(), name_str, user_search_paths_, normalizers_);
    scopes_.merge(parser.scopes_);

    // transitvely remember which files we transitively imported
    imported_.merge(parser.imported_);
    imported_.emplace(name.sym());
}

Sym Parser::parse_sym(std::string_view ctxt) {
    auto track = tracker();
    if (auto id = accept(Tok::Tag::M_id)) return id->sym();
    syntax_err("identifier", ctxt);
    return world().sym("<error>", world().dbg((Loc)track));
}

const Def* Parser::parse_type_ascr(std::string_view ctxt /*= {}*/) {
    std::string msg("type ascription of ");
    msg += ctxt;

    if (accept(Tok::Tag::T_colon)) return parse_expr(msg);
    if (ctxt.empty()) return nullptr;
    err(prev_, msg.c_str());
}

/*
 * exprs
 */

const Def* Parser::parse_expr(std::string_view ctxt, Tok::Prec p /*= Tok::Prec::Bot*/) {
    auto track = tracker();
    auto lhs   = parse_primary_expr(ctxt);
    return parse_infix_expr(track, lhs, p);
}

const Def* Parser::parse_infix_expr(Tracker track, const Def* lhs, Tok::Prec p /*= Tok::Prec::Bot*/) {
    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        if (ahead().isa(Tok::Tag::T_extract)) {
            if (auto extract = parse_extract(track, lhs, p))
                lhs = extract;
            else
                break;
        } else if (ahead().isa(Tok::Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an function type", r);
            lhs      = world().pi(lhs, rhs, track);
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            if (auto rhs = parse_expr({}, r)) {
                lhs = world().app(lhs, rhs, track); // if we can parse an expression, it's an App
            } else {
                return lhs;
            }
        }
    }

    return lhs;
}

const Def* Parser::parse_extract(Tracker track, const Def* lhs, Tok::Prec p) {
    auto [l, r] = Tok::prec(Tok::Prec::Extract);
    if (l < p) return nullptr;
    lex();

    if (ahead().isa(Tok::Tag::M_id)) {
        if (auto sigma = lhs->type()->isa_nom<Sigma>()) {
            auto sym = eat(Tok::Tag::M_id).sym();
            if (sym.is_anonymous()) err(sym.loc(), "you cannot use special symbol '_' as field access");

            auto meta = sigma->meta();
            if (meta->arity() == sigma->arity()) {
                size_t a = sigma->num_ops();
                for (size_t i = 0; i != a; ++i) {
                    if (meta->proj(a, i) == sym) return world().extract(lhs, a, i, track);
                }
            }
            err(sym.loc(), "could not find elemement '{}' to extract from '{} of type '{}'", sym, lhs, sigma);
        }
    }

    auto rhs = parse_expr("right-hand side of an extract", r);
    return world().extract(lhs, rhs, track);
}

const Def* Parser::parse_insert() {
    eat(Tok::Tag::K_ins);
    auto track = tracker();

    expect(Tok::Tag::D_paren_l, "opening paren for insert arguments");
    auto tuple = parse_expr("the tuple to insert into");
    expect(Tok::Tag::T_comma, "comma after tuple to insert into");
    auto index = parse_expr("insert index");
    expect(Tok::Tag::T_comma, "comma after insert index");
    auto value = parse_expr("insert value");
    expect(Tok::Tag::D_paren_r, "closing paren for insert arguments");

    return world().insert(tuple, index, value, track);
}

const Def* Parser::parse_primary_expr(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case DECL:                return parse_decls();
        case Tok::Tag::D_quote_l: return parse_arr();
        case Tok::Tag::D_angle_l: return parse_pack();
        case Tok::Tag::D_brace_l: return parse_block();
        case Tok::Tag::D_brckt_l: return parse_sigma();
        case Tok::Tag::D_paren_l: return parse_tuple();
        case Tok::Tag::K_Cn:      return parse_Cn();
        case Tok::Tag::K_Idx:     return parse_idx();
        case Tok::Tag::K_Type:    return parse_type();
        case Tok::Tag::K_Univ:    lex(); return world().univ();
        case Tok::Tag::K_Bool:    lex(); return world().type_bool();
        case Tok::Tag::K_Nat:     lex(); return world().type_nat();
        case Tok::Tag::K_ff:      lex(); return world().lit_ff();
        case Tok::Tag::K_tt:      lex(); return world().lit_tt();
        case Tok::Tag::T_Pi:      return parse_pi();
        case Tok::Tag::T_lam:     return parse_lam();
        case Tok::Tag::T_at:      return parse_var();
        case Tok::Tag::T_star:    lex(); return world().type();
        case Tok::Tag::T_box:     lex(); return world().type<1>();
        case Tok::Tag::T_bot:
        case Tok::Tag::T_top:
        case Tok::Tag::L_s:
        case Tok::Tag::L_u:
        case Tok::Tag::L_r:       return parse_lit();
        case Tok::Tag::M_id:      return scopes_.find(parse_sym());
        case Tok::Tag::M_i:       return lex().index();
        case Tok::Tag::K_ins:     return parse_insert();
        case Tok::Tag::M_ax: {
            // HACK hard-coded some built-in axioms
            auto tok = lex();
            auto s = tok.sym().to_string();
            if (s == "%Real"    ) return world().type_real();
            if (s == "%Wrap_add") return world().ax(Wrap::add);
            if (s == "%Wrap_sub") return world().ax(Wrap::sub);
            return scopes_.find(tok.sym());
        }
        default:
            if (ctxt.empty()) return nullptr;
            syntax_err("primary expression", ctxt);
    }
    // clang-format on
    return nullptr;
}

const Def* Parser::parse_Cn() {
    auto track = tracker();
    eat(Tok::Tag::K_Cn);
    auto dom = parse_ptrn(Tok::Tag::D_brckt_l, "domain of a continuation type");
    return world().cn(dom->type(world()), track);
}

const Def* Parser::parse_var() {
    auto track = tracker();
    eat(Tok::Tag::T_at);
    auto sym = parse_sym("variable");
    auto nom = scopes_.find(sym)->isa_nom();
    if (!nom) err(prev_, "variable must reference a nominal");
    return nom->var(track.named(sym));
}

const Def* Parser::parse_arr() {
    auto track = tracker();
    scopes_.push();
    eat(Tok::Tag::D_quote_l);

    const Def* shape = nullptr;
    Arr* arr         = nullptr;
    if (ahead(0).isa(Tok::Tag::M_id) && ahead(1).isa(Tok::Tag::T_colon)) {
        auto id = eat(Tok::Tag::M_id).sym();
        eat(Tok::Tag::T_colon);

        auto shape = parse_expr("shape of an array");
        auto type  = world().nom_infer_univ();
        arr        = world().nom_arr(type)->set_shape(shape);
        scopes_.bind(id, arr->var(world().dbg(id)));
    } else {
        shape = parse_expr("shape of an array");
    }

    expect(Tok::Tag::T_semicolon, "array");
    auto body = parse_expr("body of an array");
    expect(Tok::Tag::D_quote_r, "closing delimiter of an array");
    scopes_.pop();

    if (arr) return arr->set_body(body)->set_type(body->unfold_type());
    return world().arr(shape, body, track);
}

const Def* Parser::parse_pack() {
    // TODO This doesn't work. Rework this!
    auto track = tracker();
    scopes_.push();
    eat(Tok::Tag::D_angle_l);

    const Def* shape;
    // bool nom = false;
    if (ahead(0).isa(Tok::Tag::M_id) && ahead(1).isa(Tok::Tag::T_colon)) {
        auto sym = eat(Tok::Tag::M_id).sym();
        eat(Tok::Tag::T_colon);

        shape      = parse_expr("shape of a pack");
        auto infer = world().nom_infer(world().type_idx(shape), sym);
        scopes_.bind(sym, infer);
    } else {
        shape = parse_expr("shape of a pack");
    }

    expect(Tok::Tag::T_semicolon, "pack");
    auto body = parse_expr("body of a pack");
    expect(Tok::Tag::D_angle_r, "closing delimiter of a pack");
    scopes_.pop();
    return world().pack(shape, body, track);
}

const Def* Parser::parse_block() {
    scopes_.push();
    eat(Tok::Tag::D_brace_l);
    auto res = parse_expr("block expression");
    expect(Tok::Tag::D_brace_r, "block expression");
    scopes_.pop();
    return res;
}

const Def* Parser::parse_sigma() {
    auto track = tracker();
    auto bndr  = parse_tuple_ptrn(track, anonymous_sym());
    return bndr->type(world());
}

const Def* Parser::parse_tuple() {
    auto track = tracker();
    DefVec ops;
    parse_list("tuple", Tok::Tag::D_paren_l, [&]() { ops.emplace_back(parse_expr("tuple element")); });
    return world().tuple(ops, track);
}

const Def* Parser::parse_type() {
    auto track = tracker();
    eat(Tok::Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level  = parse_expr("type level", r);
    return world().type(level, track);
}

const Def* Parser::parse_idx() {
    eat(Tok::Tag::K_Idx);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto size   = parse_expr("size of .Idx", r);
    return world().type_idx(size);
}

const Def* Parser::parse_pi() {
    auto track = tracker();
    eat(Tok::Tag::T_Pi);
    scopes_.push();

    auto dom = parse_ptrn(Tok::Tag::D_brckt_l, "domain of a dependent function type", Tok::Prec::App);
    auto pi  = world().nom_pi(world().nom_infer_univ())->set_dom(dom->type(world()));
    auto var = pi->var(world().dbg(dom->sym()));
    dom->bind(scopes_, var);

    expect(Tok::Tag::T_arrow, "dependent function type");
    auto codom = parse_expr("codomain of a dependent function type", Tok::Prec::Arrow);
    pi->set_codom(codom);
    pi->set_type(codom->unfold_type());
    pi->set_dbg(track);

    scopes_.pop();
    return pi;
}

const Def* Parser::parse_lam() {
#if 0
    auto track = tracker();
    eat(Tok::Tag::T_lam);
    auto var = parse_sym("variable of a lambda abstraction");
    expect(Tok::Tag::T_semicolon, "lambda abstraction");
    auto type = parse_expr("type of a lambda abstraction");
#endif

    return nullptr;
}

const Def* Parser::parse_lit() {
    auto track  = tracker();
    auto lit    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);

    if (accept(Tok::Tag::T_colon)) {
        auto type = parse_expr("literal", r);

        const Def* meta = nullptr;
        // clang-format off
        switch (lit.tag()) {
            case Tok::Tag::L_s: meta = world().lit_nat('s'); break;
            case Tok::Tag::L_u: meta = world().lit_nat('u'); break;
            case Tok::Tag::L_r: meta = world().lit_nat('r'); break;
            case Tok::Tag::T_bot: return world().bot(type, track);
            case Tok::Tag::T_top: return world().top(type, track);
            default: unreachable();
        }
        // clang-format on
        return world().lit(type, lit.u(), track.meta(meta));
    }

    if (lit.tag() == Tok::Tag::T_bot) return world().bot(world().type(), track);
    if (lit.tag() == Tok::Tag::T_top) return world().top(world().type(), track);
    if (lit.tag() == Tok::Tag::L_s) err(prev_, ".Nat literal specified as signed but must be unsigned");
    if (lit.tag() == Tok::Tag::L_r) err(prev_, ".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(lit.u(), track);
}

/*
 * ptrns
 */

std::unique_ptr<Ptrn> Parser::parse_ptrn(Tok::Tag delim_l, std::string_view ctxt, Tok::Prec p /*= Tok::Prec::Bot*/) {
    auto track = tracker();
    auto sym   = anonymous_sym();
    // p ->    (p, ..., p)
    // p ->    [b, ..., b]      b ->    [b, ..., b]
    // p -> s::(p, ..., p)      b -> s::(e)
    // p -> s::[b, ..., b]      b -> s::[b, ..., b]
    // p -> s: e                b -> s: e
    // p -> s                   b ->    e
    if (ahead().isa(Tok::Tag::D_brckt_l)) {
        // p ->    [b, ..., b]      b ->    [b, ..., b]
        return parse_tuple_ptrn(track, sym);
    } else if (delim_l == Tok::Tag::D_paren_l && ahead().isa(Tok::Tag::D_paren_l)) {
        // p ->    (p, ..., p)
        return parse_tuple_ptrn(track, sym);
    } else if (ahead(0).isa(Tok::Tag::M_id)) {
        // p -> s::(p, ..., p)
        // p -> s::[b, ..., b]      b -> s::[b, ..., b]
        // p -> s: e                b -> s: e
        // p -> s                   b ->    e    where e == id
        if (ahead(1).isa(Tok::Tag::T_colon_colon)) {
            sym = eat(Tok::Tag::M_id).sym();
            eat(Tok::Tag::T_colon_colon);
            if (delim_l == Tok::Tag::D_brckt_l && ahead().isa(Tok::Tag::D_paren_l))
                err(ahead().loc(), "switching from []-style patterns to ()-style patterns is not allowed");
            // b -> s::(p, ..., p)
            // b -> s::[b, ..., b]      b -> s::[b, ..., b]
            return parse_tuple_ptrn(track, sym);
        } else if (ahead(1).isa(Tok::Tag::T_colon)) {
            // p -> s: e                b -> s: e
            sym = eat(Tok::Tag::M_id).sym();
            eat(Tok::Tag::T_colon);
            auto type = parse_expr(ctxt, p);
            return std::make_unique<IdPtrn>(track.loc(), sym, type);
        } else {
            if (delim_l == Tok::Tag::D_brckt_l) {
                // b ->    e    where e == id
                auto type = parse_expr(ctxt, p);
                return std::make_unique<IdPtrn>(track.loc(), sym, type);
            } else {
                // p -> s
                sym = eat(Tok::Tag::M_id).sym();
                return std::make_unique<IdPtrn>(track.loc(), sym, nullptr);
            }
        }
    } else if (delim_l == Tok::Tag::D_brckt_l) {
        // b ->   (e)
        // b ->    e    where e != id
        auto type = parse_expr(ctxt, p);
        return std::make_unique<IdPtrn>(track.loc(), sym, type);
    } else if (!ctxt.empty()) {
        // p -> ↯
        syntax_err("pattern", ctxt);
    }

    return nullptr;
}

std::unique_ptr<TuplePtrn> Parser::parse_tuple_ptrn(Tracker track, Sym sym) {
    auto delim_l = ahead().tag();
    std::deque<std::unique_ptr<Ptrn>> ptrns;
    std::vector<const Def*> fields;
    std::vector<Infer*> infers;
    DefVec ops;

    scopes_.push();
    parse_list("tuple pattern", delim_l, [&]() {
        auto track = tracker();
        if (!ptrns.empty()) ptrns.back()->bind(scopes_, infers.back());

        auto ptrn = parse_ptrn(delim_l, "element of a tuple pattern");
        auto type = ptrn->type(world());

        if (delim_l == Tok::Tag::D_brckt_l) {
            // If we are able to parse more stuff, we got an expression instead of just a binder.
            if (auto expr = parse_infix_expr(track, type); expr != type) {
                ptrn = std::make_unique<IdPtrn>(track.loc(), anonymous_sym(), expr);
                type = ptrn->type(world());
            }
        }

        infers.emplace_back(world().nom_infer(type, ptrn->sym()));
        fields.emplace_back(ptrn->sym().str());
        ops.emplace_back(type);
        ptrns.emplace_back(std::move(ptrn));
    });
    scopes_.pop();

    // TODO parse type
    return std::make_unique<TuplePtrn>(track.loc(), sym, std::move(ptrns), nullptr, std::move(infers));
}

/*
 * decls
 */

const Def* Parser::parse_decls(bool expr /*= true*/) {
    while (true) {
        // clang-format off
        switch (ahead().tag()) {
            case Tok::Tag::T_semicolon: lex();           break; // eat up stray semicolons
            case Tok::Tag::K_ax:        parse_ax();      break;
            case Tok::Tag::K_let:       parse_let();     break;
            case Tok::Tag::K_Sigma:
            case Tok::Tag::K_Arr:
            case Tok::Tag::K_pack:
            case Tok::Tag::K_Pi:        parse_nom();     break;
            case Tok::Tag::K_cn:
            case Tok::Tag::K_lam:       parse_nom_fun(); break;
            case Tok::Tag::K_def:       parse_def();     break;
            default:                    return expr ? parse_expr("scope of a declaration") : nullptr;
        }
        // clang-format on
    }
}

void Parser::parse_ax() {
    using namespace std::string_view_literals;
    auto track = tracker();
    eat(Tok::Tag::K_ax);
    auto ax     = expect(Tok::Tag::M_ax, "name of an axiom");
    auto ax_str = ax.sym().to_string();
    auto split  = Axiom::split(ax_str);
    if (!split) err(ax.loc(), "invalid axiom name '{}'", ax);

    auto [dialect, tag, sub] = *split;

    if (!sub.empty()) err(ax.loc(), "definition of axiom '{}' must not have sub in tag name", ax);

    auto [it, is_new] = bootstrapper_.axioms.emplace(ax_str, h::AxiomInfo{});
    auto& [key, info] = *it;
    if (is_new) {
        info.dialect = dialect;
        info.tag     = tag;
        info.tag_id  = bootstrapper_.axioms.size() - 1;
    }

    if (dialect != bootstrapper_.dialect()) {
        // TODO
        // err(ax.loc(), "axiom name `{}` implies a dialect name of `{}` but input file is named `{}`", ax,
        // info.dialect, lexer_.file());
    }

    if (bootstrapper_.axioms.size() >= std::numeric_limits<tag_t>::max())
        err(ax.loc(), "exceeded maxinum number of axioms in current dialect");

    std::deque<std::deque<std::string>> new_subs;
    if (ahead().isa(Tok::Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tok::Tag::D_paren_l, [&]() {
            auto& aliases = new_subs.emplace_back();
            aliases.emplace_back(parse_sym("tag of an axiom"));
            while (accept(Tok::Tag::T_assign)) aliases.emplace_back(parse_sym("alias of an axiom tag"));
        });
    }

    if (!is_new && new_subs.empty() && !info.subs.empty())
        err(ax.loc(), "redeclaration of axiom '{}' without specifying new subs", ax);
    else if (!is_new && !new_subs.empty() && info.subs.empty())
        err(ax.loc(), "cannot extend subs of axiom '{}' which does not have subs", ax);

    auto type = parse_type_ascr("an axiom");
    if (!is_new && info.pi != (type->isa<Pi>() != nullptr))
        err(ax.loc(), "all declarations of axiom '{}' have to be PIs if any is", ax);
    info.pi = type->isa<Pi>() != nullptr;

    auto normalizer_name = (accept(Tok::Tag::T_comma) ? parse_sym("normalizer of an axiom") : Sym()).to_string();
    if (!is_new && !(info.normalizer.empty() || normalizer_name.empty()) && info.normalizer != normalizer_name)
        err(ax.loc(), "all declarations of axiom '{}' have use the same normalizer name", ax);
    info.normalizer = normalizer_name;

    auto normalizer = [this](dialect_t d, tag_t t, sub_t s) -> Def::NormalizeFn {
        if (normalizers_)
            if (auto it = normalizers_->find(d | flags_t(t << 8u) | s); it != normalizers_->end()) return it->second;
        return nullptr;
    };

    dialect_t d = *Axiom::mangle(dialect);
    tag_t t     = info.tag_id;
    sub_t s     = info.subs.size();
    if (new_subs.empty()) {
        auto axiom = world().axiom(normalizer(d, t, 0), type, d, t, 0, track.named(ax.sym()));
        scopes_.bind(ax.sym(), axiom);
    } else {
        for (const auto& sub : new_subs) {
            auto dbg   = track.named(ax_str + "."s + sub.front());
            auto axiom = world().axiom(normalizer(d, t, s), type, d, t, s, dbg);
            for (auto& alias : sub) {
                Sym name(world().tuple_str(ax_str + "."s + alias), prev_.def(world()));
                scopes_.bind(name, axiom);
            }
            ++s;
        }
        info.subs.insert(info.subs.end(), new_subs.begin(), new_subs.end());
    }
    expect(Tok::Tag::T_semicolon, "end of an axiom");
}

void Parser::parse_let() {
    eat(Tok::Tag::K_let);
    auto ptrn = parse_ptrn(Tok::Tag::D_paren_l, "binding pattern of a let expression");
    expect(Tok::Tag::T_assign, "let expression");
    auto body = parse_expr("body of a let expression");
    ptrn->bind(scopes_, body);
    expect(Tok::Tag::T_semicolon, "let expression");
}

void Parser::parse_nom() {
    auto track    = tracker();
    auto tag      = lex().tag();
    bool external = accept(Tok::Tag::K_extern).has_value();
    auto sym      = parse_sym("nominal");
    auto type     = accept(Tok::Tag::T_colon) ? parse_expr("type of a nominal") : world().type();

    Def* nom;
    switch (tag) {
        case Tok::Tag::K_Sigma: {
            expect(Tok::Tag::T_comma, "nominal Sigma");
            auto arity = expect(Tok::Tag::L_u, "arity of a nominal Sigma");
            nom        = world().nom_sigma(type, arity.u(), track.named(sym));
            break;
        }
        case Tok::Tag::K_Arr: {
            expect(Tok::Tag::T_comma, "nominal array");
            auto shape = parse_expr("shape of a nominal array");
            nom        = world().nom_arr(type, track)->set_shape(shape);
            break;
        }
        case Tok::Tag::K_pack: nom = world().nom_pack(type, track.named(sym)); break;
        case Tok::Tag::K_Pi: {
            expect(Tok::Tag::T_comma, "nominal Pi");
            auto dom = parse_expr("domain of a nominal Pi");
            nom      = world().nom_pi(type, track.named(sym))->set_dom(dom);
            break;
        }
        default: unreachable();
    }
    scopes_.bind(sym, nom);

    scopes_.push();
    // for (auto [sym, i] : binders) scopes_.bind(sym, nom->var(i));
    if (external) nom->make_external();

    scopes_.push();
#if 0
    if (accept(Tok::Tag::T_comma)) {
        binders.clear();
        parse_var_list(binders);
        assert(binders.size() == nom->num_vars());
        for (auto [sym, i] : binders) scopes_.bind(sym, nom->var(i, world().dbg(sym)));
    }
#endif

    if (ahead().isa(Tok::Tag::T_assign))
        parse_def(sym);
    else
        expect(Tok::Tag::T_semicolon, "end of a nominal");

    scopes_.pop();
    scopes_.pop();
}

void Parser::parse_nom_fun() {
    auto track = tracker();
    auto key   = lex().tag();

    scopes_.push(); // pi scope

    bool external = accept(Tok::Tag::K_extern).has_value();
    auto sym      = parse_sym("nominal lambda");
    auto dom_p    = parse_ptrn(Tok::Tag::D_paren_l, "domain pattern of a lambda", Tok::Prec::Pi);
    auto dom_t    = dom_p->type(world());
    auto pi       = world().nom_pi(world().nom_infer_univ())->set_dom(dom_t);
    auto var_dbg  = world().dbg(dom_p->sym());
    auto pi_var   = pi->var(var_dbg);

    dom_p->bind(scopes_, pi_var);

    auto codom = key == Tok::Tag::K_cn     ? world().type_bot()
               : accept(Tok::Tag::T_arrow) ? parse_expr("return type of a lambda", Tok::Prec::Arrow)
                                           : world().nom_infer_of_infer_level();
    pi->set_codom(codom);
    pi->set_type(codom->unfold_type());
    pi->set_dbg(track);

    scopes_.pop(); // pi scope

    auto outer = scopes_.curr();
    scopes_.push(); // lam scope

    auto lam     = world().nom_lam(pi, track.named(sym));
    auto lam_var = lam->var(var_dbg);
    if (external) lam->make_external();
    scopes_.bind(outer, sym, lam);
    dom_p->bind(scopes_, lam_var);

    if (accept(Tok::Tag::T_assign)) {
        auto body = parse_expr("body of a lambda");
        lam->set(false, body);
    }
    expect(Tok::Tag::T_semicolon, "end of lambda");

    scopes_.pop(); // lam scope
}

void Parser::parse_def(Sym sym /*= {}*/) {
    if (!sym) {
        eat(Tok::Tag::K_def);
        sym = parse_sym("nominal definition");
    }

    auto nom = scopes_.find(sym)->as_nom();
    expect(Tok::Tag::T_assign, "nominal definition");

    size_t i = nom->first_dependend_op();
    size_t n = nom->num_ops();

    if (ahead().isa(Tok::Tag::D_brace_l)) {
        scopes_.push();
        parse_list("nominal definition", Tok::Tag::D_brace_l, [&]() {
            if (i == n) err(prev_, "too many operands");
            nom->set(i++, parse_expr("operand of a nominal"));
        });
        scopes_.pop();
    } else if (n - i == 1) {
        nom->set(i, parse_expr("operand of a nominal"));
    } else {
        err(prev_, "expected operands for nominal definition");
    }

    expect(Tok::Tag::T_semicolon, "end of a nominal definition");
}

} // namespace thorin::fe
