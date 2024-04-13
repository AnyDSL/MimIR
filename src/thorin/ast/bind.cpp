#include "thorin/ast/ast.h"

#include "fe/assert.h"

using namespace std::literals;

namespace thorin::ast {

using Tag = Tok::Tag;

class DummyDecl : public Decl {
public:
    DummyDecl()
        : Decl(Loc()) {}

    std::ostream& stream(Tab&, std::ostream& os) const override { return os << "<dummy>"; }
};

class Scopes {
public:
    using Scope = fe::SymMap<std::pair<Loc, const Decl*>>;

    Scopes(AST& ast)
        : ast_(ast)
        , dummy_(ast.ptr<DummyDecl>()) {
        push(); // root scope
    }

    AST& ast() const { return ast_; }
    Driver& driver() const { return ast().driver(); }
    Scope& top() { return scopes_.back(); }
    const Decl* dummy() const { return dummy_.get(); }

    void push() { scopes_.emplace_back(); }

    void pop() {
        assert(!scopes_.empty());
        scopes_.pop_back();
    }

    const Decl* find(Dbg dbg, bool quiet = false) {
        if (!dbg || dbg.sym == '_') return nullptr;

        for (auto& scope : scopes_ | std::ranges::views::reverse)
            if (auto i = scope.find(dbg.sym); i != scope.end()) return i->second.second;

        if (!quiet) {
            ast().error(dbg.loc, "'{}' not found", dbg.sym);
            bind(dbg, dummy()); // put into scope to prevent further errors
        }
        return nullptr;
    }

    void bind(Dbg dbg, const Decl* decl, bool rebind = false, bool quiet = false) {
        auto [loc, sym] = dbg;
        if (!sym || sym == '_') return; // don't do anything with '_'

        if (rebind) {
            top()[sym] = std::pair(loc, decl);
        } else if (auto [i, ins] = top().emplace(sym, std::pair(loc, decl)); !ins) {
            auto [prev_loc, prev_decl] = i->second;
            if (!quiet && !prev_decl->isa<DummyDecl>()) { // if prev_decl stems from an error - don't complain
                ast().error(loc, "redeclaration of '{}'", sym);
                ast().note(prev_loc, "previous declaration here");
            }
        }
    }

    tag_t next_tag(plugin_t p) { return plugin2tag_.emplace(p, 0).first->second; }

private:
    AST& ast_;
    Ptr<DummyDecl> dummy_;
    std::deque<Scope> scopes_;
    absl::flat_hash_map<plugin_t, tag_t> plugin2tag_;
};

/*
 * AST
 */

// clang-format off
void TypeExpr    ::bind(Scopes& s) const { level()->bind(s); }
void ErrorExpr   ::bind(Scopes&) const {}
void InferExpr   ::bind(Scopes&) const {}
void PrimaryExpr ::bind(Scopes&) const {}
// clang-format on

void IdExpr ::bind(Scopes& s) const {
    decl_ = s.find(dbg());
    assert(decl_);
}

void LitExpr::bind(Scopes& s) const {
    if (type()) {
        type()->bind(s);
        if (tag() == Tag::L_str || tag() == Tag::L_c || tag() == Tag::L_i)
            s.ast().error(type()->loc(), "a {} shall not have a type annotation", tag());
    } else {
        if (tag() == Tag::L_f) s.ast().error(loc(), "type annotation mandatory for floating point literal");
    }
}

void Module::bind(AST& ast) const {
    auto scopes = Scopes(ast);
    bind(scopes);
}

void Module::bind(Scopes& s) const {
    for (const auto& import : imports()) import->bind(s);
    decls_.bind(s);
}

void Import::bind(Scopes& s) const { module()->bind(s); }

/*
 * Ptrn
 */

void IdPtrn::bind(Scopes& s, bool quiet) const {
    if (!quiet && type()) type()->bind(s);
    s.bind(dbg(), this, rebind(), quiet);
}

void TuplePtrn::bind_decl(Scopes& s) const {}
void TuplePtrn::bind_body(Scopes& s) const {}

void TuplePtrn::bind(Scopes& s, bool quiet) const {
    for (const auto& ptrn : ptrns()) ptrn->bind(s, quiet);
    s.bind(dbg(), this, rebind(), quiet);
}

void GroupPtrn::bind(Scopes& s, bool quiet) const { s.bind(dbg(), this, rebind(), quiet); }

/*
 * Expr
 */

void BlockExpr::bind(Scopes& s) const {
    s.push();
    decls_.bind(s);
    expr()->bind(s);
    s.pop();
}

void ArrowExpr::bind(Scopes& s) const {
    dom()->bind(s);
    codom()->bind(s);
}

void PiExpr::Dom::bind(Scopes& s, bool quiet) const {
    ptrn()->bind(s, quiet);
    if (ret()) ret()->bind(s, quiet);
}

void PiExpr::bind_decl(Scopes&) const {}
void PiExpr::bind_body(Scopes&) const {}

void PiExpr::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s);
    if (codom()) {
        if (tag() == Tag::K_Cn) s.ast().error(codom()->loc(), "a continuation shall not have a codomain");
        codom()->bind(s);
    }
    s.pop();
}

void LamExpr::bind_decl(Scopes& s) const { lam()->bind_decl(s); }
void LamExpr::bind_body(Scopes& s) const { lam()->bind_body(s); }

void LamExpr::bind(Scopes& s) const {
    bind_decl(s);
    bind_body(s);
}

void AppExpr::bind(Scopes& s) const {
    callee()->bind(s);
    arg()->bind(s);
}

void RetExpr::bind(Scopes& s) const {
    callee()->bind(s);
    arg()->bind(s);
    ptrn()->bind(s);
    body()->bind(s);
}

void SigmaExpr::bind_decl(Scopes& s) const { ptrn()->bind_decl(s); }
void SigmaExpr::bind_body(Scopes& s) const { ptrn()->bind_body(s); }

void SigmaExpr::bind(Scopes& s) const {
    s.push();
    ptrn()->bind(s);
    s.pop();
}

void TupleExpr::bind(Scopes& s) const {
    for (const auto& elem : elems()) elem->bind(s);
}

template<bool arr> void ArrOrPackExpr<arr>::bind(Scopes& s) const {
    s.push();
    shape()->bind(s);
    body()->bind(s);
    s.pop();
}

template void ArrOrPackExpr<true>::bind(Scopes&) const;
template void ArrOrPackExpr<false>::bind(Scopes&) const;

void ExtractExpr::bind(Scopes& s) const {
    tuple()->bind(s);
    if (auto expr = std::get_if<Ptr<Expr>>(&index()))
        (*expr)->bind(s);
    else {
        auto dbg = std::get<Dbg>(index());
        decl_    = s.find(dbg, true);
    }
}

void InsertExpr::bind(Scopes& s) const {
    tuple()->bind(s);
    index()->bind(s);
    value()->bind(s);
}

/*
 * Decl
 */

void DeclsBlock::bind(Scopes& s) const {
    for (size_t i = 0, e = num_decls(), r = 0; true; ++i) {
        if (i < e && decl(i)->isa<RecDecl>()) {
            if (!decl(r)->isa<RecDecl>()) r = i;
        } else if (r < e && decl(r)->isa<RecDecl>()) {
            for (size_t j = r; j != i; ++j) decl(j)->as<RecDecl>()->bind_body(s);
            r = i;
        }

        if (i == e) break;
        decl(i)->bind_decl(s);
    }
}

void AxiomDecl::Alias::bind(Scopes& s, const AxiomDecl* axiom) const {
    axiom_   = axiom;
    auto sym = s.ast().sym(axiom->dbg().sym.str() + "."s + dbg().sym.str());
    full_    = Dbg(dbg().loc, sym);
    s.bind(full_, this);
}

void AxiomDecl::bind_decl(Scopes& s) const {
    type()->bind(s);

    std::tie(sym_.plugin, sym_.tag, sym_.sub) = Annex::split(s.driver(), dbg().sym);

    if (auto p = Annex::mangle(sym_.plugin)) {
        id_.plugin = *p;
        id_.tag    = s.next_tag(*p);
    } else {
        s.ast().error(dbg().loc, "invalid axiom name '{}'", dbg());
    }

    if (sym_.sub) error(dbg().loc, "axiom '{}' must not have a subtag", dbg().sym);

    if (num_subs() == 0) {
        s.bind(dbg(), this);
    } else {
        for (const auto& aliases : subs())
            for (const auto& alias : aliases) alias->bind(s, this);
    }
}

void LetDecl::bind_decl(Scopes& s) const {
    value()->bind(s);
    ptrn()->bind(s);
}

void RecDecl::bind_decl(Scopes& s) const {
    if (type()) type()->bind(s);
    if (!type()->isa<InferExpr>() && body()->isa<LamExpr>())
        s.ast().warn(type()->loc(), "type of recursive declaration ignored for function expression");

    s.bind(dbg(), this);

    if (!body()->isa<LamExpr>() && !body()->isa<PiExpr>() && !body()->isa<SigmaExpr>())
        s.ast().error(body()->loc(), "unsupported expression for a recursive declaration");
}

void RecDecl::bind_body(Scopes& s) const { body()->bind(s); }

void LamDecl::Dom::bind(Scopes& s, bool quiet) const {
    PiExpr::Dom::bind(s, quiet);

    if (filter() && !quiet) {
        if (has_bang()) {
            s.ast().error(filter()->loc(), "explicit filter specified on top of '!' annotation");
            s.ast().note(bang().loc(), "'!' here");
        }

        filter()->bind(s);
    }
}

void LamDecl::bind_decl(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s);

    if (auto bang = doms().back()->bang()) {
        if (tag() == Tag::K_lam || tag() == Tag::T_lm)
            s.ast().warn(
                bang.loc(),
                "'!' superfluous as the last curried function group of a '{}' receives a '.tt'-filter by default",
                tag());
    }
    if (auto filter = doms().back()->filter()) {
        if (auto pe = filter->isa<PrimaryExpr>()) {
            if (pe->tag() == Tag::K_tt && (tag() == Tag::K_lam || tag() == Tag::T_lm))
                s.ast().warn(filter->loc(),
                             "'.tt'-filter superfluous as the last curried function group of a '{}' receives a "
                             "'.tt' filter by default",
                             tag());
            if (pe->tag() == Tag::K_ff && (tag() != Tag::K_lam && tag() != Tag::T_lm))
                s.ast().warn(filter->loc(),
                             "'.ff'-filter superfluous as the last curried function group of a '{}' receives a "
                             "'.ff' filter by default",
                             tag());
        }
    }

    if (codom()) {
        if (tag() == Tag::K_con || tag() == Tag::K_cn)
            s.ast().error(codom()->loc(), "a continuation shall not have a codomain");
        codom()->bind(s);
    }

    s.pop();
    s.bind(dbg(), this);
}

void LamDecl::bind_body(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s, true);
    body()->bind(s);
    s.pop();
}

} // namespace thorin::ast
