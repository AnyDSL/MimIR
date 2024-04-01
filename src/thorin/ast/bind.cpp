#include "thorin/ast/ast.h"

#include "fe/assert.h"

namespace thorin::ast {

using Tag = Tok::Tag;

class DummyDecl : public Decl {
public:
    DummyDecl()
        : Decl(Loc()) {}

    std::ostream& stream(Tab&, std::ostream& os) const override { return os << "<dummy>"; }
    void bind(Scopes&) const override { fe::unreachable(); }
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
        bind(dbg, dummy_.get()); // put into scope to prevent further errors
        return nullptr;
    }

    void bind(Dbg dbg, const Decl* decl, bool rebind = false) {
        auto [loc, sym] = dbg;
        if (!sym || sym == '_') return; // don't do anything with '_'

        if (rebind) {
            top()[sym] = std::pair(loc, decl);
        } else if (auto [i, ins] = top().emplace(sym, std::pair(loc, decl)); !ins) {
            auto [prev_loc, prev_decl] = i->second;
            if (!prev_decl->isa<DummyDecl>()) { // if prev_decl stems from an error - don't complain
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
void ExtremumExpr::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void LitExpr     ::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void PiDecl      ::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void SigmaDecl   ::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void TypeExpr    ::bind(Scopes& s) const { if (level()) level()->bind(s); }
void IdExpr      ::bind(Scopes& s) const { decl_ = s.find(dbg()); }
void ErrorExpr   ::bind(Scopes&) const {}
void PrimaryExpr ::bind(Scopes&) const {}
void SigmaDecl   ::bind_rec(Scopes& s) const { body()->bind(s); }
void PiDecl      ::bind_rec(Scopes& s) const { body()->bind(s); }
// clang-format on

void Module::bind(AST& ast) const {
    auto scopes = Scopes(ast);
    bind(scopes);
}

void Module::bind(Scopes& s) const { decls_.bind(s); }

/*
 * Ptrn
 */

void IdPtrn::bind(Scopes& s) const {
    if (type()) type()->bind(s);
    s.bind(dbg(), nullptr, rebind());
}

void TuplePtrn::bind(Scopes& s) const {
    s.bind(dbg(), nullptr, rebind());
    s.push();
    for (const auto& ptrn : ptrns()) ptrn->bind(s);
    s.pop();
}

void GroupPtrn::bind(Scopes& s) const {
    type()->bind(s);
    for (auto dbg : dbgs()) s.bind(dbg, nullptr, false);
}

void ReturnPtrn::bind(Scopes& s) const {
    s.bind(dbg(), this);
    // No need to check this->type(); it's shared and has already been checked.
}

/*
 * Expr
 */

void BlockExpr::bind(Scopes& s) const {
    s.push();
    decls_.bind(s);
    if (expr()) expr()->bind(s);
    s.pop();
}

void ArrowExpr::bind(Scopes& s) const {
    dom()->bind(s);
    codom()->bind(s);
}

void PiExpr::Dom::bind(Scopes& s) const { ptrn()->bind(s); }

void PiExpr::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s);
    if (codom()) codom()->bind(s);
    s.pop();
}

void LamExpr::Dom::bind(Scopes& s) const { ptrn()->bind(s); }

void LamExpr::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s);
    if (codom()) codom()->bind(s);
    if (tag() == Tok::Tag::K_fn) {
        ret_ = s.ast().ptr<ReturnPtrn>(s.ast(), codom());
        ret_->bind(s);
    }
    body()->bind(s);
    s.pop();
}

void LamDecl::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : lam()->doms()) dom->bind(s);
    if (auto codom = lam()->codom()) codom->bind(s);
    if (lam()->tag() == Tok::Tag::K_fun) {
        lam()->ret_ = s.ast().ptr<ReturnPtrn>(s.ast(), lam()->codom());
        lam()->ret_->bind(s);
    }
    s.pop();
    s.bind(lam()->dbg(), this);
}

void LamDecl::bind_rec(Scopes& s) const {
    s.push();
    for (const auto& dom : lam()->doms()) dom->bind(s);
    if (auto ret = lam()->ret()) ret->bind(s);
    body()->bind(s);
    s.pop();
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

void SigmaExpr::bind(Scopes& s) const { ptrn()->bind(s); }

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

void LetDecl::bind(Scopes& s) const {
    value()->bind(s);
    ptrn()->bind(s);
}

void AxiomDecl::bind(Scopes& s) const { type()->bind(s); }

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

} // namespace thorin::ast
