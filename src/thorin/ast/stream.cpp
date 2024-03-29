#include <ostream>

#include "thorin/ast/ast.h"
#include "thorin/util/print.h"

namespace thorin::ast {

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
    ;
}

/*
 * Ptrn
 */

std::ostream& IdPtrn::stream(Tab& tab, std::ostream& os) const {
    os << dbg();
    if (type()) print(os, ": {}", S(tab, type()));
    return os;
}

std::ostream& GroupPtrn::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{ }: {}", dbgs(), S(tab, type()));
}

std::ostream& TuplePtrn::stream(Tab& tab, std::ostream& os) const {
    if (dbg()) print(os, "{}::", dbg());
    return print(os, "{}{, }{}", delim_l(), R(tab, ptrns()), delim_r());
}

/*
 * Ptrn
 */

std::ostream& IdExpr::stream(Tab&, std::ostream& os) const { return print(os, "{}", dbg()); }
std::ostream& PrimaryExpr::stream(Tab&, std::ostream& os) const { return print(os, "{}", tag()); }

std::ostream& LitExpr::stream(Tab& tab, std::ostream& os) const {
    os << value().lit_u();
    if (type()) print(os, ": {}", S(tab, type()));
    return os;
}

std::ostream& BlockExpr::stream(Tab& tab, std::ostream& os) const {
    tab++.println(os, "{{");
    for (auto sep = ""; const auto& decl : decls()) {
        decl->stream(tab, os << sep);
        sep = "\n";
    }
    if (expr()) expr()->stream(tab, os);
    return tab--.println(os, "}}");
}

std::ostream& TypeExpr ::stream(Tab& tab, std::ostream& os) const { return print(os, "(.Type {})", S(tab, level())); }
std::ostream& SimplePiExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{} -> {}", S(tab, dom()), S(tab, codom()));
}
std::ostream& PiExpr::Dom ::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{}{}", implicit() ? "." : "", S(tab, ptrn()));
}
std::ostream& PiExpr ::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{}{} -> {}", tag(), R(tab, doms()), S(tab, codom()));
}

std::ostream& LamExpr::Dom ::stream(Tab& tab, std::ostream& os) const {
    return print(PiExpr::Dom::stream(tab, os), "@({})", filter());
}
std::ostream& LamExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{}{} -> {} = {}", tag(), R(tab, doms()), S(tab, codom()), S(tab, body()));
}

std::ostream& AppExpr::stream(Tab& tab, std::ostream& os) const {
    return print(os, "{} {}", S(tab, callee()), S(tab, arg()));
}

std::ostream& RetExpr::stream(Tab& tab, std::ostream& os) const {
    println(os, ".ret {} = {} $ {};", S(tab, ptrn()), S(tab, callee()), S(tab, arg()));
    return tab.print(os, "{}", S(tab, body()));
}

std::ostream& SigmaExpr::stream(Tab& tab, std::ostream& os) const { return ptrn()->stream(tab, os); }
std::ostream& TupleExpr::stream(Tab& tab, std::ostream& os) const { return print(os, "({, }", R(tab, elems())); }

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

std::ostream& LetDecl::stream(Tab& tab, std::ostream& os) const {
    return print(os, ".let {} = {};", S(tab, ptrn()), S(tab, value()));
}

std::ostream& PiDecl::stream(Tab& /*tab*/, std::ostream& os) const { return print(os, ".Pi"); }
std::ostream& LamDecl::stream(Tab& tab, std::ostream& os) const { return lam()->stream(tab, os); }
std::ostream& SigmaDecl::stream(Tab& /*tab*/, std::ostream& os) const { return print(os, ".Sigma"); }

/*
 * Module
 */

std::ostream& Module::stream(Tab& tab, std::ostream& os) const {
    for (const auto& decl : decls()) tab.println(os, "{}", S(tab, decl.get()));
    return os;
}

} // namespace thorin::ast
