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
    const auto& err = ast.error();
    bind(ast);
    if (err.num_errors() != 0) throw err;
    emit(ast);

    assert(err.num_errors() == 0 && "If an error occurs during emit, an exception should have been thrown");
    if (err.num_warnings() != 0)
        ast.driver().WLOG("{} warning(s) encountered while compiling module\n{}", err.num_warnings(), err);
}

void load_plugin(World& world, View<Sym> plugins) {
    assert(!plugins.empty() && "must load at least one plugin");

    auto ast     = AST(world);
    auto parser  = Parser(ast);
    auto imports = Ptrs<Import>();
    for (auto plugin : plugins) {
        auto mod = parser.plugin(plugin);
        imports.emplace_back(ast.ptr<Import>(mod->loc(), Dbg(Loc(), plugin), Tok::Tag::K_plugin, std::move(mod)));
    }
    auto mod = ast.ptr<Module>(imports.front()->loc() + imports.back()->loc(), std::move(imports), Ptrs<ValDecl>());
    try {
        mod->compile(ast);
    } catch (const Error& err) {
        world.ELOG("{} error(s) encountered while compiling plugins: '{, }'\n{}", err.num_errors(), plugins, err);
    }
}

} // namespace thorin::ast
