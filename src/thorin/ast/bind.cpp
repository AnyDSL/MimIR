#include "thorin/ast/ast.h"

namespace thorin::ast {

class Scopes {
public:
    using Scope = fe::SymMap<std::pair<Loc, const Decl*>>;

    Scopes(AST& ast)
        : ast_(ast) {
        push(); // root scope
    }

    AST& ast() const { return ast_; }
    void push() { scopes_.emplace_back(); }
    void pop();
    Scope* curr() { return &scopes_.back(); }
    const Decl* query(Dbg) const;
    const Decl* find(Dbg) const; ///< Same as Scopes::query but potentially raises an error.
    void bind(Scope*, Dbg, const Decl*, bool rebind = false);
    void bind(Dbg dbg, const Decl* decl, bool rebind = false) { bind(&scopes_.back(), dbg, decl, rebind); }
    void swap(Scope& other) { std::swap(scopes_.back(), other); }

private:
    AST& ast_;
    std::deque<Scope> scopes_;
};

void Scopes::pop() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

const Decl* Scopes::query(Dbg dbg) const {
    if (dbg.sym == '_') return nullptr;

    for (auto& scope : scopes_ | std::ranges::views::reverse)
        if (auto i = scope.find(dbg.sym); i != scope.end()) return i->second.second;

    return nullptr;
}

const Decl* Scopes::find(Dbg dbg) const {
    if (dbg.sym == '_') ast().error(dbg.loc, "the symbol '_' is special and never binds to anything");
    if (auto res = query(dbg)) return res;
    ast().error(dbg.loc, "'{}' not found", dbg.sym);
    return nullptr;
}

void Scopes::bind(Scope* scope, Dbg dbg, const Decl* decl, bool rebind) {
    assert(dbg);
    auto [loc, sym] = dbg;
    if (sym == '_') return; // don't do anything with '_'

    if (rebind) {
        (*scope)[sym] = std::pair(loc, decl);
    } else if (auto [i, ins] = scope->emplace(sym, std::pair(loc, decl)); !ins) {
        auto prev = i->second.first;
        ast().error(loc, "redeclaration of '{}'", sym);
        ast().note(prev, "previous declaration here");
    }
}

/*
 * AST
 */

// clang-format off
void ExtremumExpr::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void LitExpr     ::bind(Scopes& s) const { if ( type())  type()->bind(s); }
void PiDecl      ::bind(Scopes& s) const { if ( type()) type()->bind(s); }
void SigmaDecl   ::bind(Scopes& s) const { if ( type()) type()->bind(s); }
void TypeExpr    ::bind(Scopes& s) const { if (level()) level()->bind(s); }
void IdExpr      ::bind(Scopes& s) const { decl_ = s.find(dbg()); }
void ErrorExpr   ::bind(Scopes&) const {}
void PrimaryExpr ::bind(Scopes&) const {}
void RecDecl     ::bind_rec(Scopes& s) const { body()->bind(s); }
// clang-format on

void Module::bind(AST& ast) const {
    auto scopes = Scopes(ast);
    bind(scopes);
}

void Module::bind(Scopes& s) const {
    for (size_t i = 0, e = num_decls(), r = 0; true; ++i) {
        if (decl(i)->isa<RecDecl>()) {
            if (!decl(r)->isa<RecDecl>()) r = i;
        } else if (decl(r)->isa<RecDecl>()) {
            for (size_t j = r; j != i; ++j) decl(j)->as<RecDecl>()->bind_rec(s);
            r = i;
        }

        if (i == e) break;
        decl(i)->bind(s);
    }
}

/*
 * Ptrn
 */

void IdPtrn::bind(Scopes& s) const {
    if (type()) type()->bind(s);
    if (dbg()) s.bind(dbg(), nullptr, rebind());
}

void TuplePtrn::bind(Scopes& s) const {
    if (dbg()) s.bind(dbg(), nullptr, rebind());
    s.push();
    for (const auto& ptrn : ptrns()) ptrn->bind(s);
    s.pop();
}

void GroupPtrn::bind(Scopes& s) const {
    type()->bind(s);
    for (auto dbg : dbgs()) s.bind(dbg, nullptr, false);
}

/*
 * Expr
 */

void BlockExpr::bind(Scopes& s) const {
    s.push();
    for (const auto& decl : decls()) decl->bind(s);
    if (expr()) expr()->bind(s);
    s.pop();
}

void SimplePiExpr::bind(Scopes& s) const {
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

void LamDecl::bind(Scopes& s) const {
    s.push();
    for (const auto& dom : lam()->doms()) dom->bind(s);
    if (auto codom = lam()->codom()) codom->bind(s);
    s.pop();
    s.bind(lam()->dbg(), this);
}

} // namespace thorin::ast
