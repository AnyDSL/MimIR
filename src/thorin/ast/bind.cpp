#include "thorin/ast/ast.h"

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
        if (dbg.is_anon()) return nullptr;

        for (auto& scope : scopes_ | std::ranges::views::reverse)
            if (auto i = scope.find(dbg.sym()); i != scope.end()) return i->second.second;

        if (!quiet) {
            ast().error(dbg.loc(), "'{}' not found", dbg.sym());
            bind(dbg, dummy()); // put into scope to prevent further errors
        }
        return nullptr;
    }

    void bind(Dbg dbg, const Decl* decl, bool rebind = false, bool quiet = false) {
        if (dbg.is_anon()) return;

        if (rebind) {
            top()[dbg.sym()] = std::pair(dbg.loc(), decl);
        } else if (auto [i, ins] = top().emplace(dbg.sym(), std::pair(dbg.loc(), decl)); !ins) {
            auto [prev_loc, prev_decl] = i->second;
            if (!quiet && !prev_decl->isa<DummyDecl>()) { // if prev_decl stems from an error - don't complain
                ast().error(dbg.loc(), "redeclaration of '{}'", dbg);
                ast().note(prev_loc, "previous declaration here");
            }
        }
    }

private:
    AST& ast_;
    Ptr<DummyDecl> dummy_;
    std::deque<Scope> scopes_;
    absl::flat_hash_map<plugin_t, tag_t> plugin2tag_;
};

/*
 * Module
 */

void Module::bind(AST& ast) const {
    auto scopes = Scopes(ast);
    bind(scopes);
}

void Module::bind(Scopes& s) const {
    for (const auto& import : implicit_imports()) import->bind(s);
    for (const auto& import : imports()) import->bind(s);
    for (const auto& decl : decls()) decl->bind(s);
}

void Import::bind(Scopes& s) const { module()->bind(s); }

/*
 * Ptrn
 */

void ErrorPtrn::bind(Scopes&, bool) const {}

void IdPtrn::bind(Scopes& s, bool quiet) const {
    if (!quiet && type()) type()->bind(s);
    s.bind(dbg(), this, rebind(), quiet);
}

void TuplePtrn::bind(Scopes& s, bool quiet) const {
    for (const auto& ptrn : ptrns()) ptrn->bind(s, quiet);
    s.bind(dbg(), this, rebind(), quiet);
}

void GrpPtrn::bind(Scopes& s, bool quiet) const { s.bind(dbg(), this, rebind(), quiet); }

/*
 * Expr
 */

// clang-format off
void IdExpr     ::bind(Scopes& s) const { decl_ = s.find(dbg()); }
void TypeExpr   ::bind(Scopes& s) const { level()->bind(s); }
void ErrorExpr  ::bind(Scopes&) const {}
void InferExpr  ::bind(Scopes&) const {}
void PrimaryExpr::bind(Scopes&) const {}
// clang-format on

void LitExpr::bind(Scopes& s) const {
    if (type()) {
        type()->bind(s);
        if (tag() == Tag::L_str || tag() == Tag::L_c || tag() == Tag::L_i)
            s.ast().error(type()->loc(), "a {} shall not have a type annotation", tag());
    } else {
        if (tag() == Tag::L_f) s.ast().error(loc(), "type annotation mandatory for floating point literal");
    }
}

void DeclExpr::bind(Scopes& s) const {
    if (is_where())
        for (const auto& decl : decls() | std::ranges::views::reverse) decl->bind(s);
    else
        for (const auto& decl : decls()) decl->bind(s);
    expr()->bind(s);
}

void ArrowExpr::bind(Scopes& s) const {
    s.push();
    if (auto sigma = dom()->isa<SigmaExpr>())
        sigma->ptrn()->bind(s);
    else
        dom()->bind(s);
    codom()->bind(s);
    s.pop();
}

void PiExpr::Dom::bind(Scopes& s, bool quiet) const {
    ptrn()->bind(s, quiet);
    if (ret()) ret()->bind(s, quiet);
}

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
    lam()->bind_decl(s);
    lam()->bind_body(s);
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

void AxiomDecl::Alias::bind(Scopes& s, const AxiomDecl* axiom) const {
    auto sym = s.ast().sym(axiom->dbg().sym().str() + "."s + dbg().sym().str());
    full_    = Dbg(dbg().loc(), sym);
    s.bind(full_, this);
}

void AxiomDecl::bind(Scopes& s) const {
    type()->bind(s);
    annex_ = s.ast().name2annex(dbg(), nullptr);

    if (annex_->fresh)
        annex_->normalizer = normalizer();
    else if (annex_->normalizer.sym() != normalizer().sym()) {
        auto l = normalizer().loc() ? normalizer().loc() : loc().anew_finis();
        s.ast().error(l, "normalizer mismatch for axiom '{}'", dbg());
        if (auto norm = annex_->normalizer)
            s.ast().note(norm.loc(), "previous normalizer '{}'", norm);
        else
            s.ast().note(l, "initially no normalizer specified");
    }

    if (num_subs() == 0) {
        s.bind(dbg(), this);
    } else {
        if (auto old = s.find(dbg(), true)) {
            if (auto old_ax = old->isa<AxiomDecl>()) {
                if (old_ax->num_subs() == 0) {
                    s.ast().error(dbg().loc(), "redeclared sub-less axiom '{}' with subs", dbg());
                    s.ast().note(old_ax->dbg().loc(), "previous location here");
                }
            }
        }
        for (const auto& aliases : subs())
            for (const auto& alias : aliases) alias->bind(s, this);
    }
}

void LetDecl::bind(Scopes& s) const {
    s.push();
    value()->bind(s);
    s.pop();
    ptrn()->bind(s);

    if (auto id = ptrn()->isa<IdPtrn>()) annex_ = s.ast().name2annex(id->dbg(), &sub_);
}

void RecDecl::bind(Scopes& s) const {
    for (auto curr = this; curr; curr = curr->next()) curr->bind_decl(s);
    for (auto curr = this; curr; curr = curr->next()) curr->bind_body(s);
    annex_ = s.ast().name2annex(dbg(), &sub_);
}

void RecDecl::bind_decl(Scopes& s) const {
    if (auto t = type()) t->bind(s);
    if (!type()->isa<InferExpr>() && body()->isa<LamExpr>())
        s.ast().warn(type()->loc(), "type of recursive declaration ignored for function expression");

    if (!body()->isa<LamExpr>() && !body()->isa<PiExpr>() && !body()->isa<SigmaExpr>())
        s.ast().error(body()->loc(), "unsupported expression for a recursive declaration");

    s.bind(dbg(), this);
}

void RecDecl::bind_body(Scopes& s) const { body()->bind(s); }

void LamDecl::Dom::bind(Scopes& s, bool quiet) const {
    PiExpr::Dom::bind(s, quiet);
    if (filter() && !quiet) filter()->bind(s);
}

void LamDecl::bind_decl(Scopes& s) const {
    s.push();
    for (size_t i = 0, e = num_doms(); i != e; ++i) dom(i)->bind(s);

    if (auto filter = doms().back()->filter()) {
        if (auto pe = filter->isa<PrimaryExpr>()) {
            if (pe->tag() == Tag::K_tt && (tag() == Tag::K_lam || tag() == Tag::T_lm))
                s.ast().warn(filter->loc(),
                             "'.tt'-filter superfluous as the last curried function group of a '{}' receives a "
                             "'.tt'-filter by default",
                             tag());
            if (pe->tag() == Tag::K_ff && (tag() != Tag::K_lam && tag() != Tag::T_lm))
                s.ast().warn(filter->loc(),
                             "'.ff'-filter superfluous as the last curried function group of a '{}' receives a "
                             "'.ff'-filter by default",
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
    annex_ = s.ast().name2annex(dbg(), &sub_);
}

void LamDecl::bind_body(Scopes& s) const {
    s.push();
    for (const auto& dom : doms()) dom->bind(s, true);
    body()->bind(s);
    s.pop();
}

void CDecl::bind(Scopes& s) const {
    s.push();
    dom()->bind(s);
    s.pop(); // we don't allow codom to depent on dom
    if (codom()) codom()->bind(s);
    s.bind(dbg(), this);
}

} // namespace thorin::ast
