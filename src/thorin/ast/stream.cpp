#include <ostream>

#include "thorin/ast/ast.h"
#include "thorin/util/print.h"

namespace thorin::ast {

using Tag = Tok::Tag;

struct S {
    S(Tab& tab, const Node* node)
        : tab(tab)
        , node(node) {}

    Tab& tab;
    const Node* node;

    friend std::ostream& operator<<(std::ostream& os, const S& s) { return s.node->stream(s.tab, os); }
};

template<class T> struct R {
    R(Tab& tab, const Ptrs<T>& range)
        : tab(tab)
        , range(range)
        , f([&tab](std::ostream& os, const Ptr<T>& ptr) { ptr->stream(tab, os); }) {}

    Tab& tab;
    const Ptrs<T>& range;
    std::function<void(std::ostream&, const Ptr<T>&)> f;
};

void Node::dump() const {
    Tab tab;
    stream(tab, std::cout) << std::endl;
}

/*
 * Ptrn
 */

std::ostream& IdPtrn::stream(Tab& tab, std::ostream& os) const {
    // clang-format off
    if ( dbg() &&  type()) return print(os, "{}: {}", dbg(), S(tab, type()));
    if ( dbg() && !type()) return print(os, "{}", dbg());
    if (!dbg() &&  type()) return print(os, "{}", S(tab, type()));
    // clang-format on
    return os << "<invalid identifier pattern>";
}

std::ostream& GroupPtrn::stream(Tab&, std::ostream& os) const { return os << dbg(); }

std::ostream& TuplePtrn::stream(Tab& tab, std::ostream& os) const {
    if (dbg()) print(os, "{}::", dbg());
    return print(os, "{}{, }{}", delim_l(), R(tab, ptrns()), delim_r());
}

/*
 * Expr
 */

std::ostream& IdExpr::stream(Tab&, std::ostream& os) const { return print(os, "{}", dbg()); }
std::ostream& ErrorExpr::stream(Tab&, std::ostream& os) const { return os << "<error>"; }
std::ostream& InferExpr::stream(Tab&, std::ostream& os) const { return os << "<infer>"; }

std::ostream& PrimaryExpr::stream(Tab&, std::ostream& os) const {
    if (tag() == Tag::M_char) return print(os, "'{}'", (char)tok().chr()); // TODO escape etc
    return print(os, "{}", tag());
}

std::ostream& LitExpr::stream(Tab& tab, std::ostream& os) const {
    os << value();
    if (type()) print(os, ": {}", S(tab, type()));
    return os;
}

std::ostream& ExtremumExpr::stream(Tab& tab, std::ostream& os) const {
    os << tag();
    if (type()) print(os, ": {}", S(tab, type()));
    return os;
}

std::ostream& BlockExpr::stream(Tab& tab, std::ostream& os) const {
    if (!has_braces()) {
        if (decls_.num_decls() == 0) return expr()->stream(tab, os);
    } else {
        os << '{';
    }

    os << std::endl;
    ++tab;
    decls_.stream(tab, os);
    (--tab).print(os, "{}", S(tab, expr()));
    if (has_braces()) tab.print(os << std::endl, "}}");
    return os;
}

std::ostream& TypeExpr::stream(Tab& tab, std::ostream& os) const { return print(os, "(.Type {})", S(tab, level())); }

std::ostream& ArrowExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{} -> {}", S(tab, dom()), S(tab, codom()));
}

std::ostream& PiExpr::Dom::stream(Tab& tab, std::ostream& os) const {
    print(os, "{}{}", is_implicit() ? "." : "", S(tab, ptrn()));
    if (ret()) print(os, " -> {}", S(tab, ret()->type()));
    return os;
}

std::ostream& PiExpr::stream(Tab& tab, std::ostream& os) const {
    print(os, "{} {}", tag(), R(tab, doms()));
    if (codom()) print(os, " -> {}", S(tab, codom()));
    return os;
}

std::ostream& LamExpr::stream(Tab& tab, std::ostream& os) const { return print(os, "{};", S(tab, lam())); }

std::ostream& AppExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{} {}", S(tab, callee()), S(tab, arg()));
}

std::ostream& RetExpr::stream(Tab& tab, std::ostream& os) const {
    println(os, ".ret {} = {} $ {};", S(tab, ptrn()), S(tab, callee()), S(tab, arg()));
    return tab.print(os, "{}", S(tab, body()));
}

std::ostream& SigmaExpr::stream(Tab& tab, std::ostream& os) const { return ptrn()->stream(tab, os); }
std::ostream& TupleExpr::stream(Tab& tab, std::ostream& os) const { return print(os, "({, })", R(tab, elems())); }

template<bool arr> std::ostream& ArrOrPackExpr<arr>::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{}{}; {}{}", arr ? "«" : "‹", S(tab, shape()), S(tab, body()), arr ? "»" : "›");
}

template std::ostream& ArrOrPackExpr<true>::stream(Tab&, std::ostream&) const;
template std::ostream& ArrOrPackExpr<false>::stream(Tab&, std::ostream&) const;

std::ostream& ExtractExpr::stream(Tab& tab, std::ostream& os) const {
    if (auto expr = std::get_if<Ptr<Expr>>(&index())) return print(os, "{}#{}", S(tab, tuple()), S(tab, expr->get()));
    return print(os, "{}#{}", S(tab, tuple()), std::get<Dbg>(index()));
}

std::ostream& InsertExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, ".ins({}, {}, {})", S(tab, tuple()), S(tab, index()), S(tab, value()));
}

/*
 * Decl
 */

std::ostream& DeclsBlock::stream(Tab& tab, std::ostream& os) const {
    for (const auto& decl : decls()) tab.println(os, "{}", S(tab, decl.get()));
    return os;
}

std::ostream& AxiomDecl::Alias::stream(Tab&, std::ostream& os) const { return os << dbg(); }

std::ostream& AxiomDecl::stream(Tab& tab, std::ostream& os) const {
    print(os, ".ax {}", dbg());
    if (num_subs() != 0) {
        os << '(';
        for (auto sep = ""; const auto& aliases : subs()) {
            print(os, "{}{ = }", sep, R(tab, aliases));
            sep = ", ";
        }
        os << ')';
    }
    print(os, ": {}", S(tab, type()));
    if (normalizer()) print(os, ", {}", normalizer());
    if (curry()) print(os, ", {}", curry());
    if (trip()) print(os, ", {}", trip());
    return os << ";";
}

std::ostream& LetDecl::stream(Tab& tab, std::ostream& os) const {
    return print(os, ".let {} = {};", S(tab, ptrn()), S(tab, value()));
}

std::ostream& RecDecl::stream(Tab& tab, std::ostream& os) const {
    print(os, ".rec {}", dbg());
    if (!type()->isa<InferExpr>()) print(os, ": {}", S(tab, type()));
    return print(os, " = {};", S(tab, body()));
}

std::ostream& LamDecl::Dom::stream(Tab& tab, std::ostream& os) const {
    if (has_bang()) os << '!';
    print(os, "{}{}", is_implicit() ? "." : "", S(tab, ptrn()));
    if (filter()) print(os, "@({})", S(tab, filter()));
    if (ret()) print(os, ": {}", S(tab, ret()->type()));
    return os;
}

std::ostream& LamDecl::stream(Tab& tab, std::ostream& os) const {
    print(os, "{} {}", tag(), dbg());
    if (num_doms() > 0 && !doms().front()->ptrn()->isa<TuplePtrn>()) os << ' ';
    print(os, "{}", R(tab, doms()));
    if (codom()) print(os, ": {}", S(tab, codom()));
    if (body()) print(os, " = {}", S(tab, body()));
    return os;
}

/*
 * Module
 */

std::ostream& Module::stream(Tab& tab, std::ostream& os) const { return decls_.stream(tab, os); }

} // namespace thorin::ast
