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

void Module::compile(AST& ast) const { bind(ast); }

#if 0
/*
 * type
 */

const Def* IdPtrn::type(World& world, Def2Fields&) const {
    if (type_) return type_;
    return type_ = world.mut_infer_type()->set(loc());
}

const Def* TuplePtrn::type(World& world, Def2Fields& def2fields) const {
    if (type_) return type_;

    auto n   = num_ptrns();
    auto ops = DefVec(n, [&](size_t i) { return ptrn(i)->type(world, def2fields); });

    if (std::ranges::all_of(ptrns_, [](auto&& b) { return b->is_anonymous(); })) {
        if (decl_) return type_ = decl_->set(ops);
        return type_ = world.sigma(ops)->set(loc());
    }

    assert(n > 0);
    auto type = world.umax<Sort::Type>(ops);

    Sigma* sigma;
    if (decl_) {
        if (auto s = decl_->isa_mut<Sigma>())
            sigma = s;
        else {
            sigma = world.mut_sigma(type, n)->set(loc(), sym());
            if (auto infer = decl_->isa<Infer>()) {
                if (infer->is_set()) {
                    if (infer->op() != sigma)
                        error(infer->loc(), "inferred different sigma '{}' for '{}'", infer->op(), sigma);
                } else {
                    infer->set(sigma);
                }
            }
        }
    } else {
        sigma = world.mut_sigma(type, n)->set(loc(), sym());
    }

    assert_emplace(def2fields, sigma, Vector<Sym>(n, [this](size_t i) { return ptrn(i)->sym(); }));

    sigma->set(0, ops[0]);
    for (size_t i = 1; i != n; ++i) {
        if (auto infer = infers_[i - 1]) infer->set(sigma->var(n, i - 1)->set(ptrn(i - 1)->sym()));
        sigma->set(i, ops[i]);
    }

    auto var = sigma->var()->as<Var>();
    VarRewriter rw(var, var);
    sigma->reset(0, ops[0]);
    for (size_t i = 1; i != n; ++i) sigma->reset(i, rw.rewrite(ops[i]));

    if (!decl_) {
        if (auto imm = sigma->immutabilize()) return type_ = imm;
    }

    return type_ = sigma;
}
#endif

} // namespace thorin::ast
