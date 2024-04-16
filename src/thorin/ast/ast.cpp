#include "thorin/ast/ast.h"

#include "thorin/ast/parser.h"

namespace thorin::ast {

LamExpr::LamExpr(Ptr<LamDecl>&& lam)
    : Expr(lam->loc())
    , lam_(std::move(lam)) {}

/*
 * Ptrn::to_expr/to_ptrn
 */

Ptr<Expr> Ptrn::to_expr(AST& ast, Ptr<Ptrn>&& ptrn) {
    if (auto idp = ptrn->isa<IdPtrn>(); idp && !idp->dbg() && idp->type()) {
        if (auto ide = idp->type()->isa<IdExpr>()) return ast.ptr<IdExpr>(ide->dbg());
    } else if (auto tuple = ptrn->isa<TuplePtrn>(); tuple && tuple->is_brckt()) {
        (void)ptrn.release();
        return ast.ptr<SigmaExpr>(Ptr<TuplePtrn>(tuple));
    }
    return {};
}

Ptr<Ptrn> Ptrn::to_ptrn(Ptr<Expr>&& expr) {
    if (auto sigma = expr->isa<SigmaExpr>())
        return std::move(const_cast<SigmaExpr*>(sigma)->ptrn_); // TODO get rid off const_cast
    return {};
}

void Module::compile(AST& ast) const {
    bind(ast);
    if (ast.error().num_errors() != 0) throw ast.error();
    emit(ast);
    // HACK
    if (ast.error().num_errors() == 0 && ast.error().num_msgs() != 0) std::cerr << ast.error();
}

void load_plugin(World& world, Sym plugin) {
    auto ast    = AST(world);
    auto parser = Parser(ast);
    auto mod    = parser.plugin(plugin);
    mod->compile(ast);
}

} // namespace thorin::ast
