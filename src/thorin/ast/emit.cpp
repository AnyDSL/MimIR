#include "thorin/rewrite.h"

#include "thorin/ast/ast.h"

namespace thorin::ast {

using Tag = Tok::Tag;

class Emitter {
public:
    Emitter(AST& ast, World& world)
        : ast_(ast)
        , world_(world) {}

    AST& ast() const { return ast_; }
    World& world() { return world_; }
    Driver& driver() { return world().driver(); }

    absl::node_hash_map<Sigma*, fe::SymMap<size_t>, GIDHash<const Def*>, GIDEq<const Def*>> sigma2sym2idx;

private:
    AST& ast_;
    World& world_;
};

/*
 * AST
 */

Ref ErrorExpr::emit(Emitter&) const { fe::unreachable(); }
Ref InferExpr::emit(Emitter& e) const { return e.world().type_infer_univ(); }

Ref IdExpr::emit(Emitter&) const {
    assert(decl());
    return decl()->def();
}

Ref ExtremumExpr::emit(Emitter& e) const {
    auto t = type() ? type()->emit(e) : e.world().type<0>();
    return (tag() == Tag::T_bot ? e.world().bot(t) : e.world().top(t))->set(loc());
}

Ref LitExpr::emit(Emitter& e) const {
    int base        = 10;
    auto [loc, sym] = value();
    auto c_str      = sym.c_str();
    size_t b        = 0;
    bool sign       = false;

    if (sym[b] == '+')
        ++b;
    else if (sym[b] == '-')
        ++b, sign = true;

    if (sym.size() == b) error(loc, "signed literal '{}' without value", value());

    if (sym.size() > b + 1) {
        switch (sym[b + 1]) {
                // clang-format off
            case 'b':
            case 'B': base =  2, b += 2; break;
            case 'o':
            case 'O': base =  8, b += 2; break;
            case 'x':
            case 'X': base = 16, b += 2; break;
            default:                     break;
                // clang-format on
        }
    }

    char* end;
    auto val = std::strtoull(c_str + b, &end, base);

    if (*end == '\0') {
        if (sign) error(loc, "negative .Nat literal '{}' not allowed", value());
        if (type()) return e.world().lit(type()->emit(e), val);
        return e.world().lit_nat(val)->set(loc);
    }

    if (*end == 'i' || *end == 'I') {
        auto width = std::strtoull(end + 1, nullptr, base);
        if (width != 64 && val >= Idx::bitwidth2size(width))
            error(loc, "value '{}' doesn't fit in given bit width '{}'", val, width);
        return e.world().lit_int(width, val)->set(loc);
    }

    return {};
}

Ref TypeExpr::emit(Emitter& e) const {
    auto l = level()->emit(e);
    return e.world().type(l)->set(loc());
}

Ref PrimaryExpr ::emit(Emitter& e) const {
    switch (tag()) {
            // clang-format off
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
        case Tag::M_char: return e.world().lit_i8(tok().chr());
        case Tag::T_star: return e.world().type<0>();
        case Tag::T_box:  return e.world().type<1>();
        default: fe::unreachable();
            // clang-format on
    }
}

void Module::emit(AST& ast, World& world) const {
    auto emitter = Emitter(ast, world);
    emit(emitter);
}

void Module::emit(Emitter& e) const {
    for (const auto& import : imports()) import.emit(e);
    decls_.emit(e);
}

void Import::emit(Emitter& e) const { module()->emit(e); }

/*
 * Ptrn::emit_value
 */

Ref IdPtrn::emit_value(Emitter&, Ref def) const { return def_ = def->set(dbg()); }

Ref TuplePtrn::emit_value(Emitter& e, Ref def) const {
    for (size_t i = 0, n = num_ptrns(); i != n; ++i) ptrn(i)->emit_value(e, def->proj(n, i));
    return def_ = def->set(dbg());
}

Ref GroupPtrn::emit_value(Emitter&, Ref def) const { return def_ = def; }

/*
 * Ptrn::emit_Type
 */

Ref IdPtrn::emit_type(Emitter& e) const { return type() ? type()->emit(e) : e.world().mut_infer_type()->set(loc()); }

Ref GroupPtrn::emit_type(Emitter& e) const { return id()->emit_type(e); }

Ref TuplePtrn::emit_type(Emitter& e) const {
    auto n        = num_ptrns();
    auto type     = e.world().type_infer_univ();
    auto sigma    = e.world().mut_sigma(type, n)->set(loc(), dbg().sym);
    auto var      = sigma->var()->set(dbg());
    auto& sym2idx = e.sigma2sym2idx[sigma];

    for (size_t i = 0; i != n; ++i) {
        sigma->set(i, ptrn(i)->emit_type(e));
        ptrn(i)->emit_value(e, var->proj(n, i));
        sym2idx[ptrn(i)->dbg().sym] = i;
    }

    if (auto imm = sigma->immutabilize()) return imm;
    return sigma;
}

/*
 * Expr
 */

Ref BlockExpr::emit(Emitter& e) const {
    decls_.emit(e);
    return expr()->emit(e);
}

Ref ArrowExpr::emit(Emitter& e) const {
    auto d = dom()->emit(e);
    auto c = codom()->emit(e);
    return e.world().pi(d, c)->set(loc());
}

void PiExpr::Dom::emit_type(Emitter& e) const {
    pi_       = decl_ ? decl_ : e.world().mut_pi(e.world().type_infer_univ(), is_implicit());
    auto type = ptrn()->emit_type(e);

    if (ret()) {
        auto ret_t   = e.world().cn(ret()->emit_type(e));
        auto sigma_t = e.world().umax<Sort::Type>({type, ret_t});
        auto sigma   = e.world().mut_sigma(sigma_t, 2)->set(ret()->loc());
        auto var     = sigma->var()->set(ret()->loc().anew_begin());
        sigma->set(0, type);
        ptrn()->emit_value(e, var->proj(2, 0));
        sigma->set(1, ret_t);

        if (auto imm = sigma->immutabilize())
            type = imm;
        else
            type = sigma;
        pi_->set_dom(type);
    } else {
        pi_->set_dom(type);
        ptrn()->emit_value(e, pi_->var());
    }
}

Ref PiExpr::emit_decl(Emitter& e, Ref type) const {
    const auto& first   = doms().front();
    return first->decl_ = e.world().mut_pi(type, first->is_implicit());
}

void PiExpr::emit_body(Emitter& e, Ref) const { emit(e); }

Ref PiExpr::emit(Emitter& e) const {
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

Ref LamExpr::emit(Emitter& e) const {
    auto res = emit_decl(e, {});
    emit_body(e, {});
    return res;
}

Ref AppExpr::emit(Emitter& e) const {
    auto c = callee()->emit(e);
    auto a = arg()->emit(e);
    return e.world().iapp(c, a)->set(loc());
}

Ref RetExpr::emit(Emitter& e) const {
    auto c = callee()->emit(e);
    if (auto cn = Pi::ret_pi(c->type())) {
        auto con  = e.world().mut_lam(cn);
        auto pair = e.world().tuple({arg()->emit(e), con});
        auto app  = e.world().app(c, pair)->set(c->loc() + arg()->loc());
        ptrn()->emit_value(e, con->var());
        con->set(false, body()->emit(e));
        return app;
    }

    error(c->loc(), "callee of a .ret expression must type as a returning continuation but got '{}' of type '{}'", c,
          c->type());
}

Ref SigmaExpr::emit_decl(Emitter&, Ref) const { return {}; }
void SigmaExpr::emit_body(Emitter&, Ref) const {}

Ref SigmaExpr::emit(Emitter& e) const { return ptrn()->emit_type(e); }

Ref TupleExpr::emit(Emitter& e) const {
    DefVec elems(num_elems(), [&](size_t i) { return elem(i)->emit(e); });
    return e.world().tuple(elems)->set(loc());
}

template<bool arr> Ref ArrOrPackExpr<arr>::emit(Emitter& e) const {
    auto s = shape()->emit_type(e);
    if (shape()->is_anon()) {
        auto b = body()->emit(e);
        return arr ? e.world().arr(s, b) : e.world().pack(s, b);
    }
    auto t = e.world().type_infer_univ();
    auto a = e.world().mut_arr(t); // TODO packs
    a->set_shape(s);
    auto var = a->var();
    shape()->emit_value(e, var);
    return a->set_body(body()->emit(e));
}

template Ref ArrOrPackExpr<true>::emit(Emitter&) const;
template Ref ArrOrPackExpr<false>::emit(Emitter&) const;

Ref ExtractExpr::emit(Emitter& e) const {
    auto tup = tuple()->emit(e);
    if (auto dbg = std::get_if<Dbg>(&index())) {
        if (auto sigma = tup->type()->isa_mut<Sigma>()) {
            if (auto i = e.sigma2sym2idx.find(sigma); i != e.sigma2sym2idx.end()) {
                auto sigma          = i->first->as_mut<Sigma>();
                const auto& sym2idx = i->second;
                if (auto i = sym2idx.find(dbg->sym); i != sym2idx.end())
                    return e.world().extract(tup, sigma->num_ops(), i->second)->set(loc());
            }
        }

        if (decl()) return e.world().extract(tup, decl()->def())->set(loc());
        error(dbg->loc, "cannot resolve index '{}' for extraction", dbg);
    }

    auto expr = std::get<Ptr<Expr>>(index()).get();
    auto i    = expr->emit(e);
    return e.world().extract(tup, i)->set(loc());
}

Ref InsertExpr::emit(Emitter& e) const {
    auto t = tuple()->emit(e);
    auto i = index()->emit(e);
    auto v = value()->emit(e);
    return e.world().insert(t, i, v)->set(loc());
}

/*
 * Decl
 */

void DeclsBlock::emit(Emitter& e) const {
    for (size_t i = 0, n = num_decls(), r = 0; true; ++i) {
        if (i < n && decl(i)->isa<RecDecl>()) {
            if (!decl(r)->isa<RecDecl>()) r = i;
        } else if (r < n && decl(r)->isa<RecDecl>()) {
            for (size_t j = r; j != i; ++j) decl(j)->as<RecDecl>()->emit_body(e);
            r = i;
        }

        if (i == n) break;
        decl(i)->emit_decl(e);
    }
}

void LetDecl::emit_decl(Emitter& e) const {
    auto v = value()->emit(e);
    def_   = ptrn()->emit_value(e, v);
}

void AxiomDecl::Alias::emit(Emitter& e) const {
    auto norm      = e.driver().normalizer(axiom_->id_.plugin, axiom_->id_.tag, 0);
    const auto& id = axiom_->id_;
    def_           = e.world().axiom(norm, *id.curry, *id.trip, axiom_->thorin_type_, id.plugin, id.tag, 0)->set(dbg());
}

void AxiomDecl::emit_decl(Emitter& e) const {
    auto [plugin, tag, sub] = Annex::split(e.driver(), dbg().sym);
    auto&& [annex, is_new]  = e.driver().name2annex(dbg().sym, plugin, tag, dbg().loc);
    thorin_type_            = type()->emit(e);
    auto [curry, trip]      = Axiom::infer_curry_and_trip(thorin_type_);

    if (id_.curry) {
        if (*id_.curry > curry) error(curry_.loc, "curry counter cannot be greater than {}", curry);
    } else {
        id_.curry = curry;
    }

    if (!id_.trip) id_.trip = trip;

    if (num_subs() == 0) {
        auto norm = e.driver().normalizer(id_.plugin, id_.tag, 0);
        def_      = e.world().axiom(norm, curry, trip, thorin_type_, id_.plugin, id_.tag, 0)->set(dbg());
    } else {
        for (const auto& aliases : subs())
            for (const auto& alias : aliases) alias->emit(e);
    }
}

void RecDecl::emit_decl(Emitter& e) const {
    auto t = type() ? type()->emit(e) : e.world().type_infer_univ();
    def_   = body()->emit_decl(e, t);

#if 0
    if (auto sigma = body()->isa<SigmaExpr>()) {
        def_ = e.world().mut_sigma(t, sigma->ptrn()->num_ptrns());
    } else if (auto pi = body()->isa<PiExpr>()) {
        def_ = e.world().mut_pi(t);
    } else if (auto lam = body()->isa<LamExpr>()) {
#    if 0
        if (auto infer = t->isa<Infer>()) t = e.world().mut_pi(t, e.world().type_infer_univ());
        if (auto pi = t->isa<Pi>())
            def_ = e.world().mut_lam(pi);
        else
            error(type()->loc(), "type of a function must be a function type");
#    endif
        def_ = body()->emit_decl(e);
    }
#endif
}

void RecDecl::emit_body(Emitter& e) const { body()->emit_body(e, def_); }

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
    // Iterate over all doms: Build a Lam for cur dom, by furst building a curried Pi for the remaining doms.
    for (size_t i = 0, n = num_doms(); i != n; ++i) {
        for (const auto& dom : doms() | std::ranges::views::drop(i)) dom->emit_type(e);
        auto cod = codom() ? codom()->emit(e) : e.world().type_bot();

        for (const auto& dom : doms() | std::ranges::views::drop(i) | std::ranges::views::reverse) {
            dom->pi_->set_codom(cod);
            cod = dom->pi_;
        }

        auto cur = dom(i);
        auto lam = cur->emit_value(e);
        auto f   = cur->has_bang()                             ? e.world().lit_tt()
                 : cur->filter()                               ? cur->filter()->emit(e)
                 : (tag() == Tag::T_lm || tag() == Tag::K_lam) ? e.world().lit_tt()
                                                               : e.world().lit_ff();
        lam->set_filter(f);

        if (i == 0)
            def_ = lam->set(loc(), dbg().sym);
        else
            dom(i - 1)->lam_->set_body(lam);
    }
}

void LamDecl::emit_body(Emitter& e) const {
    doms().back()->lam_->set_body(body()->emit(e));
    if (is_external()) doms().front()->lam_->make_external();
}

} // namespace thorin::ast
