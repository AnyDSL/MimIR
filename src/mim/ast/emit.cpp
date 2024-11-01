#include "mim/ast/ast.h"

using namespace std::literals;

namespace mim::ast {

using Tag = Tok::Tag;

class Emitter {
public:
    Emitter(AST& ast)
        : ast_(ast) {}

    AST& ast() const { return ast_; }
    World& world() { return ast().world(); }
    Driver& driver() { return world().driver(); }

    void register_annex(AnnexInfo* annex, sub_t sub, Ref def) {
        if (annex) {
            const auto& id = annex->id;
            world().register_annex(id.plugin | (id.tag << 8) | sub, def);
        }
    }

    absl::node_hash_map<Sigma*, fe::SymMap<size_t>, GIDHash<const Def*>, GIDEq<const Def*>> sigma2sym2idx;

private:
    AST& ast_;
};

/*
 * Module
 */

void Module::emit(AST& ast) const {
    auto emitter = Emitter(ast);
    emit(emitter);
}

void Module::emit(Emitter& e) const {
    auto _ = e.world().push(loc());
    for (const auto& import : implicit_imports()) import->emit(e);
    for (const auto& import : imports()) import->emit(e);
    for (const auto& decl : decls()) decl->emit(e);
}

void Import::emit(Emitter& e) const { module()->emit(e); }

/*
 * Ptrn::emit_value
 */

Ref Ptrn::emit_value(Emitter& e, Ref def) const {
    auto _ = e.world().push(loc());
    emit_type(e);
    emit_value_(e, def);
    return def_ = def->set(dbg());
}

void TuplePtrn::emit_value_(Emitter& e, Ref def) const {
    for (size_t i = 0, n = num_ptrns(); i != n; ++i) ptrn(i)->emit_value(e, def->proj(n, i));
}

/*
 * Ptrn::emit_Type
 */

Ref ErrorPtrn::emit_type(Emitter&) const { fe::unreachable(); }

Ref IdPtrn::emit_type(Emitter& e) const {
    auto _ = e.world().push(loc());
    return type() ? type()->emit(e) : e.world().mut_infer_type();
}

Ref GrpPtrn::emit_type(Emitter& e) const { return id()->emit_type(e); }

Ref TuplePtrn::emit_type(Emitter& e) const { return emit_body(e, {}); }

Ref TuplePtrn::emit_body(Emitter& e, Ref decl) const {
    auto _ = e.world().push(loc());
    auto n = num_ptrns();
    Sigma* sigma;
    if (decl) {
        sigma = decl->as_mut<Sigma>();
    } else {
        auto type = e.world().type_infer_univ();
        sigma     = e.world().mut_sigma(type, n)->set(dbg().sym());
    }
    auto var      = sigma->var()->set(dbg());
    auto& sym2idx = e.sigma2sym2idx[sigma];

    for (size_t i = 0; i != n; ++i) {
        sigma->set(i, ptrn(i)->emit_type(e));
        ptrn(i)->emit_value(e, var->proj(n, i));
        sym2idx[ptrn(i)->dbg().sym()] = i;
    }

    if (auto imm = sigma->immutabilize()) return imm;
    return sigma;
}

Ref TuplePtrn::emit_decl(Emitter& e, Ref type) const {
    auto _ = e.world().push(loc());
    type   = type ? type : e.world().type_infer_univ();
    return e.world().mut_sigma(type, num_ptrns())->set(dbg().sym());
}

/*
 * Expr
 */

Ref Expr::emit(Emitter& e) const {
    auto _ = e.world().push(loc());
    return emit_(e);
}

Ref ErrorExpr::emit_(Emitter&) const { fe::unreachable(); }
Ref InferExpr::emit_(Emitter& e) const { return e.world().mut_infer_type(); }

Ref IdExpr::emit_(Emitter&) const {
    assert(decl());
    return decl()->def();
}

Ref TypeExpr::emit_(Emitter& e) const {
    auto l = level()->emit(e);
    return e.world().type(l);
}

Ref PrimaryExpr ::emit_(Emitter& e) const {
    // clang-format off
    switch (tag()) {
        case Tag::K_Univ: return e.world().univ();
        case Tag::K_Nat:  return e.world().type_nat();
        case Tag::K_Idx:  return e.world().type_idx();
        case Tag::K_Bool: return e.world().type_bool();
        case Tag::K_ff:   return e.world().lit_ff();
        case Tag::K_tt:   return e.world().lit_tt();
        case Tag::K_i1:   return e.world().lit_i1();
        case Tag::K_i8:   return e.world().lit_i8();
        case Tag::K_i16:  return e.world().lit_i16();
        case Tag::K_i32:  return e.world().lit_i32();
        case Tag::K_i64:  return e.world().lit_i64();
        case Tag::K_I1:   return e.world().type_i1();
        case Tag::K_I8:   return e.world().type_i8();
        case Tag::K_I16:  return e.world().type_i16();
        case Tag::K_I32:  return e.world().type_i32();
        case Tag::K_I64:  return e.world().type_i64();
        case Tag::T_star: return e.world().type<0>();
        case Tag::T_box:  return e.world().type<1>();
        default: fe::unreachable();
    }
    // clang-format on
}

Ref LitExpr::emit_(Emitter& e) const {
    auto t = type() ? type()->emit(e) : nullptr;
    // clang-format off
    switch (tag()) {
        case Tag::L_f:
        case Tag::L_s:
        case Tag::L_u:   return t ? e.world().lit(t, tok().lit_u()) : e.world().lit_nat(tok().lit_u());
        case Tag::L_i:   return tok().lit_i();
        case Tag::L_c:   return e.world().lit_i8(tok().lit_c());
        case Tag::L_str: return e.world().tuple(tok().sym());
        case Tag::T_bot: return t ? e.world().bot(t) : e.world().type_bot();
        case Tag::T_top: return t ? e.world().top(t) : e.world().type_top();
        default: fe::unreachable();
    }
    // clang-format on
}

Ref DeclExpr::emit_(Emitter& e) const {
    if (is_where())
        for (const auto& decl : decls() | std::ranges::views::reverse) decl->emit(e);
    else
        for (const auto& decl : decls()) decl->emit(e);
    return expr()->emit(e);
}

Ref ArrowExpr::emit_decl(Emitter& e, Ref type) const { return decl_ = e.world().mut_pi(type, false)->set(loc()); }

void ArrowExpr::emit_body(Emitter& e, Ref) const {
    decl_->set_dom(dom()->emit(e));
    decl_->set_codom(codom()->emit(e));
}

Ref ArrowExpr::emit_(Emitter& e) const {
    auto d = dom()->emit(e);
    auto c = codom()->emit(e);
    return e.world().pi(d, c);
}

void PiExpr::Dom::emit_type(Emitter& e) const {
    pi_        = decl_ ? decl_ : e.world().mut_pi(e.world().type_infer_univ(), implicit());
    auto dom_t = ptrn()->emit_type(e);

    if (ret()) {
        auto sigma = e.world().mut_sigma(2)->set(loc());
        auto var   = sigma->var()->set(ret()->loc().anew_begin());
        sigma->set(0, dom_t);
        ptrn()->emit_value(e, var->proj(2, 0));
        auto ret_t = e.world().cn(ret()->emit_type(e));
        sigma->set(1, ret_t);

        if (auto imm = sigma->immutabilize())
            dom_t = imm;
        else
            dom_t = sigma;
        pi_->set_dom(dom_t);
    } else {
        pi_->set_dom(dom_t);
        ptrn()->emit_value(e, pi_->var());
    }
}

Ref PiExpr::emit_decl(Emitter& e, Ref type) const {
    const auto& first   = doms().front();
    return first->decl_ = e.world().mut_pi(type, first->implicit())->set(loc());
}

void PiExpr::emit_body(Emitter& e, Ref) const { emit(e); }

Ref PiExpr::emit_(Emitter& e) const {
    for (const auto& dom : doms()) dom->emit_type(e);

    auto cod = codom() ? codom()->emit(e) : e.world().type_bot();
    for (const auto& dom : doms() | std::ranges::views::reverse) {
        dom->pi_->set_codom(cod);
        cod = dom->pi_;
    }

    return doms().front()->pi_;
}

Ref LamExpr::emit_decl(Emitter& e, Ref) const { return lam()->emit_decl(e), lam()->def(); }
void LamExpr::emit_body(Emitter& e, Ref) const { lam()->emit_body(e); }

Ref LamExpr::emit_(Emitter& e) const {
    auto res = emit_decl(e, {});
    emit_body(e, {});
    return res;
}

Ref AppExpr::emit_(Emitter& e) const {
    auto c = callee()->emit(e);
    auto a = arg()->emit(e);
    return is_explicit() ? e.world().app(c, a) : e.world().iapp(c, a);
}

Ref RetExpr::emit_(Emitter& e) const {
    auto c = callee()->emit(e);
    if (auto cn = Pi::ret_pi(c->type())) {
        auto con  = e.world().mut_lam(cn);
        auto pair = e.world().tuple({arg()->emit(e), con});
        auto app  = e.world().app(c, pair)->set(c->loc() + arg()->loc());
        ptrn()->emit_value(e, con->var());
        con->set(false, body()->emit(e));
        return app;
    }

    error(c->loc(), "callee of a ret expression must type as a returning continuation but got '{}' of type '{}'", c,
          c->type());
}

Ref SigmaExpr::emit_decl(Emitter& e, Ref type) const { return ptrn()->emit_decl(e, type); }
void SigmaExpr::emit_body(Emitter& e, Ref decl) const { ptrn()->emit_body(e, decl); }
Ref SigmaExpr::emit_(Emitter& e) const { return ptrn()->emit_type(e); }

Ref TupleExpr::emit_(Emitter& e) const {
    DefVec elems(num_elems(), [&](size_t i) { return elem(i)->emit(e); });
    return e.world().tuple(elems);
}

template<bool arr> Ref ArrOrPackExpr<arr>::emit_(Emitter& e) const {
    auto s = shape()->emit_type(e);
    if (shape()->dbg().is_anon()) { // immutable
        auto b = body()->emit(e);
        return arr ? e.world().arr(s, b) : e.world().pack(s, b);
    }

    auto t = e.world().type_infer_univ();
    auto a = e.world().mut_arr(t);
    a->set_shape(s);

    if (arr) {
        auto var = a->var();
        shape()->emit_value(e, var);
        a->set_body(body()->emit(e));
        if (auto imm = a->immutabilize()) return imm;
        return a;
    } else {
        auto p   = e.world().mut_pack(a);
        auto var = p->var();
        shape()->emit_value(e, var);
        auto b = body()->emit(e);
        a->set_body(b->type());
        p->set(b);
        if (auto imm = p->immutabilize()) return imm;
        return p;
    }
}

template Ref ArrOrPackExpr<true>::emit_(Emitter&) const;
template Ref ArrOrPackExpr<false>::emit_(Emitter&) const;

Ref ExtractExpr::emit_(Emitter& e) const {
    auto tup = tuple()->emit(e);
    if (auto dbg = std::get_if<Dbg>(&index())) {
        if (auto sigma = tup->type()->isa_mut<Sigma>()) {
            if (auto i = e.sigma2sym2idx.find(sigma); i != e.sigma2sym2idx.end()) {
                auto sigma          = i->first->as_mut<Sigma>();
                const auto& sym2idx = i->second;
                if (auto i = sym2idx.find(dbg->sym()); i != sym2idx.end())
                    return e.world().extract(tup, sigma->num_ops(), i->second);
            }
        }

        if (decl()) return e.world().extract(tup, decl()->def());
        error(dbg->loc(), "cannot resolve index '{}' for extraction", dbg);
    }

    auto expr = std::get<Ptr<Expr>>(index()).get();
    auto i    = expr->emit(e);
    return e.world().extract(tup, i);
}

Ref InsertExpr::emit_(Emitter& e) const {
    auto t = tuple()->emit(e);
    auto i = index()->emit(e);
    auto v = value()->emit(e);
    return e.world().insert(t, i, v);
}

/*
 * Decl
 */

void AxiomDecl::emit(Emitter& e) const {
    mim_type_ = type()->emit(e);
    auto& id  = annex_->id;

    std::tie(id.curry, id.trip) = Axiom::infer_curry_and_trip(mim_type_);
    if (curry_) {
        if (curry_.lit_u() > id.curry)
            error(curry_.loc(), "curry counter cannot be greater than {}", id.curry);
        else
            id.curry = curry_.lit_u();
    }

    if (trip_) {
        if (trip_.lit_u() > id.curry)
            error(trip_.loc(), "trip counter cannot be greater than curry counter '{}'", (int)id.curry);
        else
            id.trip = trip_.lit_u();
    }

    auto pi = mim_type_->isa<Pi>();
    if (!annex_->pi)
        annex_->pi = pi;
    else if (bool(pi) ^ bool(*annex_->pi))
        error(dbg().loc(), "all declarations of annex '{}' have to be function types if any is", dbg().sym());

    if (num_subs() == 0) {
        auto norm  = e.driver().normalizer(id.plugin, id.tag, 0);
        auto axiom = e.world().axiom(norm, id.curry, id.trip, mim_type_, id.plugin, id.tag, 0)->set(dbg());
        def_       = axiom;
        e.world().register_annex(id.plugin, id.tag, 0, axiom);
    } else {
        sub_t offset = annex_->subs.size();
        for (sub_t i = 0, n = num_subs(); i != n; ++i) {
            auto& aliases = annex_->subs.emplace_back(std::deque<Sym>());
            sub_t s       = i + offset;
            auto norm     = e.driver().normalizer(id.plugin, id.tag, s);
            auto name     = e.world().sym(dbg().sym().str() + "."s + sub(i).front()->dbg().sym().str());
            auto axiom    = e.world().axiom(norm, id.curry, id.trip, mim_type_, id.plugin, id.tag, s)->set(name);
            e.world().register_annex(id.plugin, id.tag, s, axiom);

            for (const auto& alias : sub(i)) {
                alias->def_ = axiom;
                aliases.emplace_back(alias->dbg().sym());
            }
        }
    }
}

void LetDecl::emit(Emitter& e) const {
    auto v = value()->emit(e);
    def_   = ptrn()->emit_value(e, v);
    e.register_annex(annex_, sub_, def_);
}

void RecDecl::emit(Emitter& e) const {
    for (auto curr = this; curr; curr = curr->next()) curr->emit_decl(e);
    for (auto curr = this; curr; curr = curr->next()) curr->emit_body(e);
}

void RecDecl::emit_decl(Emitter& e) const {
    auto t = type() ? type()->emit(e) : e.world().type_infer_univ();
    def_   = body()->emit_decl(e, t);
    def_->set(dbg());
}

void RecDecl::emit_body(Emitter& e) const {
    body()->emit_body(e, def_);
    // TODO immutabilize?
    e.register_annex(annex_, sub_, def_);
}

Lam* LamDecl::Dom::emit_value(Emitter& e) const {
    lam_     = e.world().mut_lam(pi_);
    auto var = lam_->var();

    if (ret()) {
        ptrn()->emit_value(e, var->proj(2, 0));
        ret()->emit_value(e, var->proj(2, 1));
    } else {
        ptrn()->emit_value(e, var);
    }

    return lam_;
}

void LamDecl::emit_decl(Emitter& e) const {
    auto _      = e.world().push(loc());
    bool is_cps = tag_ == Tag::K_cn || tag_ == Tag::K_con || tag_ == Tag::K_fn || tag_ == Tag::K_fun;

    // Iterate over all doms: Build a Lam for cur dom, by first building a curried Pi for the remaining doms.
    for (size_t i = 0, n = num_doms(); i != n; ++i) {
        for (const auto& dom : doms() | std::ranges::views::drop(i)) dom->emit_type(e);
        auto cod = codom() ? codom()->emit(e) : is_cps ? e.world().type_bot() : e.world().mut_infer_type();

        for (const auto& dom : doms() | std::ranges::views::drop(i) | std::ranges::views::reverse) {
            dom->pi_->set_codom(cod);
            cod = dom->pi_;
        }

        auto cur    = dom(i);
        auto lam    = cur->emit_value(e);
        auto filter = cur->filter()        ? cur->filter()->emit(e)
                    : i + 1 == n && is_cps ? e.world().lit_ff()
                                           : e.world().lit_tt();
        lam->set_filter(filter);

        if (i == 0)
            def_ = lam->set(dbg().sym());
        else
            dom(i - 1)->lam_->set_body(lam);
    }
}

void LamDecl::emit_body(Emitter& e) const {
    doms().back()->lam_->set_body(body()->emit(e));
    if (is_external()) doms().front()->lam_->make_external();
    e.register_annex(annex_, sub_, def_);
}

void CDecl::emit(Emitter& e) const {
    auto dom_t = dom()->emit_type(e);
    if (tag() == Tag::K_cfun) {
        auto ret_t = codom()->emit(e);
        def_       = e.world().mut_fun(dom_t, ret_t)->set(dbg());
    } else {
        def_ = e.world().mut_con(dom_t)->set(dbg());
    }
}

} // namespace mim::ast
