#include "thorin/ast/ast.h"

#include "thorin/ast/parser.h"

namespace thorin::ast {

AST::~AST() {
    assert(error().num_errors() == 0 && "please encounter any errors before destroying this class");
    if (error().num_warnings() != 0)
        driver().WLOG("{} warning(s) encountered while compiling module\n{}", error().num_warnings(), error());
}

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
    ast.error().ack();
    emit(ast);
}

AST load_plugins(World& world, View<Sym> plugins) {
    auto tag = Tok::Tag::K_import;
    if (!world.driver().flags().bootstrap) {
        for (auto plugin : plugins) world.driver().load(plugin);
        tag = Tok::Tag::K_plugin;
    }

    auto ast     = AST(world);
    auto parser  = Parser(ast);
    auto imports = Ptrs<Import>();

    for (auto plugin : plugins)
        if (auto mod = parser.import(plugin))
            imports.emplace_back(ast.ptr<Import>(mod->loc(), tag, Dbg(Loc(), plugin), std::move(mod)));

    if (!plugins.empty()) {
        auto mod = ast.ptr<Module>(imports.front()->loc() + imports.back()->loc(), std::move(imports), Ptrs<ValDecl>());
        // TODO return mod
    }

    return ast;
}

} // namespace thorin::ast
