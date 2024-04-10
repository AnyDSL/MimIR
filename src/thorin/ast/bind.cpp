#include "thorin/ast/ast.h"

#include "fe/assert.h"

using namespace std::string_literals;

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
    Scope& top() { return scopes_.back(); }
    const Decl* dummy() const { return dummy_.get(); }

    void push() { scopes_.emplace_back(); }

    void pop() {
        assert(!scopes_.empty());
        scopes_.pop_back();
    }

    const Decl* find(Dbg dbg) {
        if (!dbg || dbg.sym == '_') return nullptr;

        for (auto& scope : scopes_ | std::ranges::views::reverse)
            if (auto i = scope.find(dbg.sym); i != scope.end()) return i->second.second;

        ast().error(dbg.loc, "'{}' not found", dbg.sym);
        bind(dbg, dummy()); // put into scope to prevent further errors
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

private:
    AST& ast_;
    Ptr<DummyDecl> dummy_;
    std::deque<Scope> scopes_;
};

/*
 * AST
 */

// clang-format off
void ExtremumExpr::bind(Scopes& s) const { if (type()) type()->bind(s); }
void LitExpr     ::bind(Scopes& s) const { if (type()) type()->bind(s); }
void TypeExpr    ::bind(Scopes& s) const { level()->bind(s); }
void IdExpr      ::bind(Scopes& s) const { decl_ = s.find(dbg()); }
void ErrorExpr   ::bind(Scopes&) const {}
void InferExpr   ::bind(Scopes&) const {}
void PrimaryExpr ::bind(Scopes&) const {}
// clang-format on

void Module::bind(AST& ast) const {
    auto scopes = Scopes(ast);
    bind(scopes);
}

void Module::bind(Scopes& s) const {
    for (const auto& import : imports()) import.bind(s);
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

void TuplePtrn::bind(Scopes& s, bool quiet) const {
    for (const auto& ptrn : ptrns()) ptrn->bind(s, quiet);
    s.bind(dbg(), this, rebind(), quiet);
}

void GroupPtrn::bind(Scopes& s, bool quiet) const { s.bind(dbg(), this, rebind(), quiet); }

void ReturnPtrn::bind(Scopes& s, bool quiet) const {
    s.bind(dbg(), this, rebind(), quiet);
    // No need to check this->type(); it's shared and has already been checked.
}

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

void PiExpr::Dom::bind(Scopes& s, bool quiet) const { ptrn()->bind(s, quiet); }

void PiExpr::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s);
    if (codom()) {
        if (tag() == Tag::K_Cn) s.ast().error(codom()->loc(), "a continuation shall not have a codomain");
        codom()->bind(s);
    }
    s.pop();
}

void LamExpr::bind(Scopes& s) const {
    lam()->bind(s);
    lam()->bind_rec(s);
}

void AppExpr::bind(Scopes& s) const {
    callee()->bind(s);
    arg()->bind(s);
}

void RetExpr::bind(Scopes& s) const {
    ptrn()->bind(s);
    callee()->bind(s);
    arg()->bind(s);
}

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
    if (auto expr = std::get_if<Ptr<Expr>>(&index())) (*expr)->bind(s);
    // We check for Dbg case during "emit" as we need full type info.
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
            for (size_t j = r; j != i; ++j) decl(j)->as<RecDecl>()->bind_rec(s);
            r = i;
        }

        if (i == e) break;
        decl(i)->bind(s);
    }
}

void AxiomDecl::bind(Scopes& s) const {
    type()->bind(s);

    if (num_subs() == 0) {
        s.bind(dbg(), this);
    } else {
        for (const auto& aliases : subs()) {
            for (const auto& alias : aliases) {
                auto sym = s.ast().sym(dbg().sym.str() + "."s + alias.sym.str());
                s.bind(Dbg(dbg().loc, sym), this);
            }
        }
    }
}

void LetDecl::bind(Scopes& s) const {
    value()->bind(s);
    ptrn()->bind(s);
}

void RecDecl::bind(Scopes& s) const {
    if (type()) type()->bind(s);
    s.bind(dbg(), this);
}

void RecDecl::bind_rec(Scopes& s) const { body()->bind(s); }

void LamDecl::Dom::bind(Scopes& s, bool quiet) const {
    ptrn()->bind(s, quiet);
    if (ret()) ret()->bind(s);

    if (filter() && !quiet) {
        if (has_bang()) {
            s.ast().error(filter()->loc(), "explicit filter specified on top of '!' annotation");
            s.ast().note(bang().loc(), "'!' here");
        }

        filter()->bind(s);
    }
}

void LamDecl::bind(Scopes& s) const {
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

void LamDecl::bind_rec(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s, true);
    if (ret()) ret()->bind(s, true);
    body()->bind(s);
    s.pop();
}

} // namespace thorin::ast
