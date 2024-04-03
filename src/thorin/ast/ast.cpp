#include "thorin/ast/ast.h"

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/driver.h"
#include "thorin/rewrite.h"
#include "thorin/world.h"

namespace thorin::ast {

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

void Module::compile(AST& ast, World& world) const {
    bind(ast);
    emit(ast, world);
}

} // namespace thorin::ast
