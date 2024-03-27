#pragma once

#include <deque>
#include <memory>

#include "thorin/def.h"

#include "thorin/ast/tok.h"

#include "fe/arena.h"
#include "fe/cast.h"

namespace thorin {

class Driver;
class Infer;
class Sigma;
class World;

using Def2Fields = DefMap<Vector<Sym>>;

namespace ast {

template<class T> using Ptr  = fe::Arena::Ptr<const T>;
template<class T> using Ptrs = std::deque<Ptr<T>>;

class AST {
public:
    AST(World& world);

    World& world() { return world_; }
    Driver& driver();
    Sym anonymous() const { return anonymous_; }

    /// @name Sym
    ///@{
    Sym sym(std::string_view);
    Sym sym(const char*);
    Sym sym(const std::string&);
    ///@}

    template<class T, class... Args> auto ptr(Args&&... args) {
        return arena_.mk<const T>(*this, std::forward<Args&&>(args)...);
    }

private:
    World& world_;
    fe::Arena arena_;
    Sym anonymous_;
};

class Scopes;

class Node : public fe::RuntimeCast<Node> {
protected:
    Node(AST& ast, Loc loc)
        : ast_(ast)
        , loc_(loc) {}
    virtual ~Node() {}

public:
    AST& ast() const { return ast_; }
    Loc loc() const { return loc_; }

private:
    AST& ast_;
    Loc loc_;
};

class Expr : public Node {
protected:
    Expr(AST& ast, Loc loc)
        : Node(ast, loc) {}
};

class Decl : public Node {
protected:
    Decl(AST& ast, Loc loc, Sym sym)
        : Node(ast, loc)
        , sym_(sym) {}

public:
    Sym sym() const { return sym_; }

private:
    Sym sym_;
};

/*
 * Ptrn
 */

class Ptrn : public Node {
public:
    Ptrn(AST& ast, Loc loc, bool rebind, Sym sym, Ptr<Expr>&& type)
        : Node(ast, loc)
        , rebind_(rebind)
        , sym_(sym)
        , type_(std::move(type)) {}

    bool rebind() const { return rebind_; }
    Sym sym() const { return sym_; }
    Dbg dbg() const { return {loc(), sym()}; }
    bool is_anonymous() const { return sym() == '_'; }

    [[nodiscard]] static Ptr<Expr> expr(AST& ast, Ptr<Ptrn>&&);

    virtual Ptr<Expr> expr() const = 0;
    // virtual void bind(Scopes&, const Def*, bool rebind = false) const = 0;
    // virtual const Def* type(World&, Def2Fields&) const                = 0;

protected:
    bool rebind_;
    Sym sym_;
    Ptr<Expr> type_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(AST& ast, Loc loc, bool rebind, Sym sym, Ptr<Expr>&& type)
        : Ptrn(ast, loc, rebind, sym, std::move(type)) {}
    IdPtrn(AST& ast, Ptr<Expr>&& type)
        : Ptrn(ast, type->loc(), false, ast.anonymous(), std::move(type)) {}

    Ptr<Expr> expr() const override;
    // void bind(Scopes&, const Def*, bool rebind = false) const override {}
    // const Def* type(World&, Def2Fields&) const override {}
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(AST& ast, Loc loc, bool rebind, Sym sym, Tok::Tag tag, Ptrs<Ptrn>&& ptrns, Ptr<Expr>&& type)
        : Ptrn(ast, loc, rebind, sym, std::move(type))
        , tag_(tag)
        , ptrns_(std::move(ptrns)) {}

    Tok::Tag tag() const { return tag_; }
    const auto& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    Ptr<Expr> expr() const override;
    // void bind(Scopes&, const Def*, bool rebind = false) const override {}
    // const Def* type(World&, Def2Fields&) const override {}

private:
    Tok::Tag tag_;
    Ptrs<Ptrn> ptrns_;
};

/*
 * Expr
 */

class IdExpr : public Expr {
public:
    IdExpr(AST& ast, Loc loc, Sym sym)
        : Expr(ast, loc)
        , sym_(sym) {}
    IdExpr(AST& ast, Tok tok)
        : IdExpr(ast, tok.loc(), tok.sym()) {}

    Sym sym() const { return sym_; }

private:
    Sym sym_;
};

class PrimaryExpr : public Expr {
public:
    PrimaryExpr(AST& ast, Tok tok)
        : Expr(ast, tok.loc())
        , tag_(tok.tag()) {}

    Tok::Tag tag() const { return tag_; }

private:
    Tok::Tag tag_;
};

class BlockExpr : public Expr {
public:
    BlockExpr(AST& ast, Loc loc, Ptrs<Decl>&& decls, Ptr<Expr>&& expr)
        : Expr(ast, loc)
        , decls_(std::move(decls))
        , expr_(std::move(expr)) {}

    const auto& decls() const { return decls_; }
    const Decl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }
    const Expr* expr() const { return expr_.get(); }

private:
    Ptrs<Decl> decls_;
    Ptr<Expr> expr_;
};

// lam

class SimplePiExpr : public Expr {
public:
    SimplePiExpr(AST& ast, Loc loc, Ptr<Expr>&& dom, Ptr<Expr>&& codom)
        : Expr(ast, loc)
        , dom_(std::move(dom))
        , codom_(std::move(codom)) {}

private:
    const Expr* dom() const { return dom_.get(); }
    const Expr* codom() const { return codom_.get(); }

private:
    Ptr<Expr> dom_;
    Ptr<Expr> codom_;
};

class PiExpr : public Expr {
public:
    class Dom : public Node {
    public:
        Dom(AST& ast, Loc loc, bool implicit, Ptr<Ptrn>&& ptrn)
            : Node(ast, loc)
            , implicit_(implicit)
            , ptrn_(std::move(ptrn)) {}

        bool implicit() const { return implicit_; }
        const Ptrn* ptrn() const { return ptrn_.get(); }

    private:
        bool implicit_;
        Ptr<Ptrn> ptrn_;
    };

    PiExpr(AST& ast, Loc loc, Tok::Tag tag, Ptrs<Dom>&& doms, Ptr<Expr>&& codom)
        : Expr(ast, loc)
        , tag_(tag)
        , doms_(std::move(doms))
        , codom_(std::move(codom)) {}

private:
    Tok::Tag tag() const { return tag_; }
    const auto& doms() const { return doms_; }
    const Dom* dom(size_t i) const { return doms_[i].get(); }
    size_t num_doms() const { return doms_.size(); }
    const Expr* codom() const { return codom_.get(); }

private:
    Tok::Tag tag_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
};

class LamExpr : public Expr {
public:
    class Dom : public PiExpr::Dom {
    public:
        Dom(AST& ast, Loc loc, bool implicit, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& filter)
            : PiExpr::Dom(ast, loc, implicit, std::move(ptrn))
            , filter_(std::move(filter)) {}

        const Expr* filter() const { return filter_.get(); }

    private:
        Ptr<Expr> filter_;
    };

    LamExpr(AST& ast, Loc loc, Tok::Tag tag, Sym sym, Ptrs<Dom>&& doms, Ptr<Expr>&& codom, Ptr<Expr>&& body)
        : Expr(ast, loc)
        , tag_(tag)
        , sym_(sym)
        , doms_(std::move(doms))
        , codom_(std::move(codom))
        , body_(std::move(body)) {}

    Tok::Tag tag() const { return tag_; }
    Sym sym() const { return sym_; }
    const auto& doms() const { return doms_; }
    const Dom* dom(size_t i) const { return doms_[i].get(); }
    const Expr* codom() const { return codom_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Tok::Tag tag_;
    Sym sym_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
    Ptr<Expr> body_;
    ;
};

class AppExpr : public Expr {
public:
    AppExpr(AST& ast, Loc loc, bool is_explicit, Ptr<Expr>&& callee, Ptr<Expr>&& arg)
        : Expr(ast, loc)
        , is_explicit_(is_explicit)
        , callee_(std::move(callee))
        , arg_(std::move(arg)) {}

    bool is_explicit() const { return is_explicit_; }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }

private:
    bool is_explicit_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
};

class RetExpr : public Expr {
public:
    RetExpr(AST& ast, Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& callee, Ptr<Expr>&& arg, Ptr<Expr> body)
        : Expr(ast, loc)
        , ptrn_(std::move(ptrn))
        , callee_(std::move(callee))
        , arg_(std::move(arg))
        , body_(std::move(body)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
    Ptr<Expr> body_;
};
// tuple

class SigmaExpr : public Expr {
public:
    SigmaExpr(AST& ast, Ptr<TuplePtrn>&& ptrn)
        : Expr(ast, ptrn->loc())
        , ptrn_(std::move(ptrn)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }

private:
    Ptr<TuplePtrn> ptrn_;
};

class TupleExpr : public Expr {
public:
    TupleExpr(AST& ast, Loc loc, Ptrs<Expr>&& elems)
        : Expr(ast, loc)
        , elems_(std::move(elems)) {}

    const auto& elems() const { return elems_; }
    const Expr* elem(size_t i) const { return elems_[i].get(); }
    size_t num_elems() const { return elems().size(); }

private:
    Ptrs<Expr> elems_;
};

template<bool arr> class ArrOrPackExpr : public Expr {
public:
    ArrOrPackExpr(AST& ast, Loc loc, Sym sym, Ptr<Expr>&& shape, Ptr<Expr>&& body)
        : Expr(ast, loc)
        , sym_(sym)
        , shape_(std::move(shape))
        , body_(std::move(body)) {}

    Sym sym() const { return sym_; }
    const Expr* shape() const { return shape_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Sym sym_;
    Ptr<Expr> shape_;
    Ptr<Expr> body_;
};

using ArrExpr  = ArrOrPackExpr<true>;
using PackExpr = ArrOrPackExpr<false>;

class ExtractExpr : public Expr {
public:
    ExtractExpr(AST& ast, Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index)
        : Expr(ast, loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }

private:
    Ptr<Expr> tuple_;
    Ptr<Expr> index_;
};

class InsertExpr : public Expr {
public:
    InsertExpr(AST& ast, Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index, Ptr<Expr>&& value)
        : Expr(ast, loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index))
        , value_(std::move(value)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }
    const Expr* value() const { return value_.get(); }

private:
    Ptr<Expr> tuple_;
    Ptr<Expr> index_;
    Ptr<Expr> value_;
};

/*
 * Further Decls
 */

class LetDecl : public Decl {
public:
    LetDecl(AST& ast, Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& value)
        : Decl(ast, loc, ptrn->sym())
        , ptrn_(std::move(ptrn))
        , value_(std::move(value)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* value() const { return value_.get(); }

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> value_;
};

class AxiomDecl : public Decl {
public:
    AxiomDecl(AST& ast, Loc loc, Sym sym)
        : Decl(ast, loc, sym) {}
};

class PiDecl : public Decl {
public:
    PiDecl(AST& ast, Loc loc, Sym sym, Ptr<Expr>&& type, Ptr<Expr>&& body)
        : Decl(ast, loc, sym)
        , type_(std::move(type))
        , body_(std::move(body)) {}

    const Expr* type() const { return type_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Ptr<Expr> type_;
    Ptr<Expr> body_;
};

class LamDecl : public Decl {
public:
    LamDecl(AST& ast, Ptr<LamExpr>&& lam)
        : Decl(ast, lam->loc(), lam->sym())
        , lam_(std::move(lam)) {}

    const LamExpr* lam() const { return lam_.get(); }

private:
    Ptr<LamExpr> lam_;
};

class SigmaDecl : public Decl {
public:
    SigmaDecl(AST& ast, Loc loc, Sym sym, Ptr<Expr>&& type, Ptr<Expr>&& body)
        : Decl(ast, loc, sym)
        , type_(std::move(type))
        , body_(std::move(body)) {}

    const Expr* type() const { return type_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Ptr<Expr> type_;
    Ptr<Expr> body_;
};

/*
 * Module
 */

class Module : public Node {
public:
    Module(AST& ast, Loc loc, Ptrs<Decl>&& decls)
        : Node(ast, loc)
        , decls_(std::move(decls)) {}

    const auto& decls() const { return decls_; }
    const Decl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }

private:
    Ptrs<Decl> decls_;
};

} // namespace ast
} // namespace thorin
