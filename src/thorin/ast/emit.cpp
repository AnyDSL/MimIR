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
    World& world() const { return world_; }

private:
    AST& ast_;
    World& world_;
    DefMap<Vector<Dbg>> fields_;
};

/*
 * AST
 */

Ref ErrorExpr::emit(Emitter&) const { fe::unreachable(); }
Ref InferExpr::emit(Emitter&) const { fe::unreachable(); }

Ref IdExpr::emit(Emitter&) const { return decl_->def(); }

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
        case Tag::M_char: return nullptr; // TODO
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

Ref IdPtrn::emit_value(Emitter&, Ref def) const { return def; }

Ref TuplePtrn::emit_value(Emitter& e, Ref def) const {
    for (size_t i = 0, n = num_ptrns(); i != n; ++i) ptrn(i)->emit_value(e, def->proj(n, i));
    return {};
}

Ref GroupPtrn::emit_value(Emitter&, Ref def) const { return def; }

Ref ReturnPtrn::emit_value(Emitter& e, Ref def) const {
    type()->emit(e);
    return def;
}

/*
 * Ptrn::emit_Type
 */

Ref IdPtrn::emit_type(Emitter& e) const {
    return thorin_type_ = type() ? type()->emit(e) : e.world().mut_infer_type()->set(loc());
}

Ref GroupPtrn::emit_type(Emitter& e) const { return id()->emit_type(e); }

Ref TuplePtrn::emit_type(Emitter& e) const {
    auto n   = num_ptrns();
    auto ops = DefVec(n, [&](size_t i) { return ptrn(i)->emit_type(e); });

    if (std::ranges::all_of(ptrns_, [](auto&& b) { return b->is_anon(); })) return e.world().sigma(ops)->set(loc());

    assert(n > 0);
    auto type  = e.world().umax<Sort::Type>(ops);
    auto sigma = e.world().mut_sigma(type, n)->set(loc(), dbg().sym);

    // assert_emplace(def2fields, sigma, Vector<Sym>(n, [this](size_t i) { return ptrn(i)->sym(); }));

    sigma->set(0, ops[0]);
    for (size_t i = 1; i != n; ++i) {
        // if (auto infer = infers_[i - 1]) infer->set(sigma->var(n, i - 1)->set(ptrn(i - 1)->sym()));
        sigma->set(i, ops[i]);
    }

    auto var = sigma->var()->as<Var>();
    VarRewriter rw(var, var);
    sigma->reset(0, ops[0]);
    for (size_t i = 1; i != n; ++i) sigma->reset(i, rw.rewrite(ops[i]));

    if (auto imm = sigma->immutabilize()) return imm;
    return thorin_type_ = sigma;
}

Ref ReturnPtrn::emit_type(Emitter&) const { fe::unreachable(); }

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

Ref PiExpr::Dom::emit(Emitter&) const {
    // ptrn()->emit(e);
    return {};
}

Ref PiExpr::emit(Emitter& e) const {
    for (const auto& dom : doms()) dom->emit(e);
    codom()->emit(e);
    return {};
}

Ref LamExpr::emit(Emitter& e) const {
    lam()->emit(e);
    lam()->emit_rec(e);
    return lam()->def();
}

Ref AppExpr::emit(Emitter& e) const {
    auto c = callee()->emit(e);
    auto a = arg()->emit(e);
    return e.world().app(c, a)->set(loc());
}

Ref RetExpr::emit(Emitter& e) const {
    // ptrn()->emit(e);
    callee()->emit(e);
    arg()->emit(e);
    return {};
}

Ref SigmaExpr::emit(Emitter&) const {
    // ptrn()->emit(e);
    return {};
}

Ref TupleExpr::emit(Emitter& e) const {
    DefVec elems(num_elems(), [&](size_t i) { return elem(i)->emit(e); });
    return e.world().tuple(elems)->set(loc());
}

template<bool arr> Ref ArrOrPackExpr<arr>::emit(Emitter& e) const {
    /*auto s =*/shape()->emit(e);
    if (dbg() && dbg().sym != '_') {
        // auto mut = arr ? e.world().mut_arr(
        body()->emit(e);
    }
    return {};
}

template Ref ArrOrPackExpr<true>::emit(Emitter&) const;
template Ref ArrOrPackExpr<false>::emit(Emitter&) const;

Ref ExtractExpr::emit(Emitter& e) const {
    auto t = tuple()->emit(e);
    if (auto expr = std::get_if<Ptr<Expr>>(&index())) {
        auto i = (*expr)->emit(e);
        return e.world().extract(t, i)->set(loc());
    }

    // auto dbg = std::get<Dbg>(index());
    return {};
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
            for (size_t j = r; j != i; ++j) decl(j)->as<RecDecl>()->emit_rec(e);
            r = i;
        }

        if (i == n) break;
        decl(i)->emit(e);
    }
}

void LetDecl::emit(Emitter& e) const {
    auto v = value()->emit(e);
    def_   = ptrn()->emit_value(e, v);
}

void AxiomDecl::emit(Emitter& e) const {
    type()->emit(e);

    if (num_subs() == 0) {
    } else {
        for (const auto& aliases : subs()) {
            for (const auto& alias : aliases) {
                outln("{}", alias);
                // auto sym = s.ast().sym(dbg().sym.str() + "."s + alias.sym.str());
            }
        }
    }
}

void RecDecl::emit(Emitter& e) const {
    if (type()) type()->emit(e);
}

void RecDecl::emit_rec(Emitter& e) const { body()->emit(e); }

void LamDecl::Dom::emit(Emitter& e) const {
    auto dom_t     = ptrn()->emit_type(e);
    auto pi        = e.world().mut_pi(e.world().type_infer_univ(), is_implicit())->set_dom(dom_t);
    thorin_.filter = has_bang() ? e.world().lit_tt() : filter()->emit(e);
    thorin_.lam    = e.world().mut_lam(pi);
}

void LamDecl::emit(Emitter& e) const {
    for (const auto& dom : doms()) dom->emit(e);
    codom()->emit(e);

    if (is_external()) doms().front()->thorin().lam->make_external();
}

void LamDecl::emit_rec(Emitter& e) const {
    for (const auto& dom : doms()) dom->ptrn()->emit_value(e, dom->thorin().lam->var());
    // if (auto ret = lam()->ret()) ret->emit(e);
    body()->emit(e);
}

} // namespace thorin::ast
