#include <string>
#include <string_view>

#include "thorin/ast/ast.h"

#include "fe/assert.h"

using namespace std::string_literals;

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
 * Ptrn
 */

Ref IdPtrn::emit_(Emitter&, Ref def) const { return def; }

Ref TuplePtrn::emit_(Emitter& e, Ref def) const {
    for (size_t i = 0, n = num_ptrns(); i != n; ++i) ptrn(i)->emit_(e, def->proj(n, i));
    return {};
}

Ref GroupPtrn::emit_(Emitter&, Ref def) const { return def; }

Ref ReturnPtrn::emit_(Emitter& e, Ref def) const {
    type()->emit(e);
    return def;
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

Ref PiExpr::Dom::emit(Emitter&) const {
    // ptrn()->emit(e);
    return {};
}

Ref PiExpr::emit(Emitter& e) const {
    for (const auto& dom : doms()) dom->emit(e);
    codom()->emit(e);
    return {};
}

Ref LamExpr::Dom::emit(Emitter&) const {
    // ptrn()->emit(e);
    return {};
}

Ref LamExpr::emit(Emitter& e) const {
    for (const auto& dom : doms()) dom->emit(e);
    codom()->emit(e);
    body()->emit(e);
    return {};
}

Ref LamDecl::emit_(Emitter& e) const {
    for (const auto& dom : lam()->doms()) dom->emit(e);
    lam()->codom()->emit(e);
    return {};
}

void LamDecl::emit_rec(Emitter& e) const {
    for (const auto& dom : lam()->doms()) dom->emit(e);
    // if (auto ret = lam()->ret()) ret->emit(e);
    body()->emit(e);
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

Ref LetDecl::emit_(Emitter& e) const {
    auto v = value()->emit(e);
    ptrn()->emit(e, v);
    return v;
}

Ref AxiomDecl::emit_(Emitter& e) const {
    type()->emit(e);

    if (num_subs() == 0) {
    } else {
        for (const auto& aliases : subs()) {
            for (const auto& alias : aliases) {
                // auto sym = s.ast().sym(dbg().sym.str() + "."s + alias.sym.str());
            }
        }
    }
    return {};
}

Ref PiDecl::emit_(Emitter& e) const {
    if (type()) type()->emit(e);
    return {};
}

void PiDecl::emit_rec(Emitter& e) const { body()->emit(e); }

Ref SigmaDecl::emit_(Emitter& e) const {
    if (type()) type()->emit(e);
    return {};
}

void SigmaDecl::emit_rec(Emitter& e) const { body()->emit(e); }

} // namespace thorin::ast
