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
        return arena_.mk<const T>(std::forward<Args&&>(args)...);
    }

private:
    World& world_;
    fe::Arena arena_;
    Sym anonymous_;
};

class Scopes;

class Node : public fe::RuntimeCast<Node> {
protected:
    Node(Loc loc)
        : loc_(loc) {}
    virtual ~Node() {}

public:
    Loc loc() const { return loc_; }

    virtual std::ostream& stream(Tab&, std::ostream&) const = 0;

private:
    Loc loc_;
};

class Expr : public Node {
protected:
    Expr(Loc loc)
        : Node(loc) {}
};

class Decl : public Node {
protected:
    Decl(Loc loc, Sym sym)
        : Node(loc)
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
    Ptrn(Loc loc)
        : Node(loc) {}

    bool rebind() const { return rebind_; }
    Sym sym() const { return sym_; }
    Dbg dbg() const { return {loc(), sym()}; }
    bool is_anonymous() const { return sym() == '_'; }
    const Expr* type() const { return type_.get(); }

    [[nodiscard]] static Ptr<Expr> expr(AST&, Ptr<Ptrn>&&);
    // virtual void bind(Scopes&, const Def*, bool rebind = false) const = 0;
    // virtual const Def* type(World&, Def2Fields&) const                = 0;

protected:
    bool rebind_;
    Sym sym_;
    Ptr<Expr> type_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, bool rebind, Sym sym, Ptr<Expr>&& type)
        : Ptrn(loc)
        , rebind_(rebind)
        , sym_(sym)
        , type_(std::move(type)) {}

    bool rebind() const { return rebind_; }
    Sym sym() const { return sym_; }

    static Ptr<IdPtrn> mk_type(AST& ast, Ptr<Expr>&& type) {
        return ast.ptr<IdPtrn>(type->loc(), false, ast.anonymous(), std::move(type));
    }

    // void bind(Scopes&, const Def*, bool rebind = false) const override {}
    // const Def* type(World&, Def2Fields&) const override {}
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    bool rebind_;
    Sym sym_;
    Ptr<Expr> type_;
};

/// `sym_0 ... sym_n-1 : type`
class GroupPtrn : public Ptrn {
public:
    GroupPtrn(Loc loc, std::deque<Sym>&& syms, Ptr<Expr>&& type)
        : Ptrn(loc)
        , syms_(std::move(syms))
        , type_(std::move(type)) {}

    const auto& syms() const { return syms_; }
    const Expr* type() const { return type_.get(); }

    static Ptr<IdPtrn> mk_type(AST& ast, Ptr<Expr>&& type) {
        return ast.ptr<IdPtrn>(type->loc(), false, ast.anonymous(), std::move(type));
    }

    // void bind(Scopes&, const Def*, bool rebind = false) const override {}
    // const Def* type(World&, Def2Fields&) const override {}
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    std::deque<Sym> syms_;
    Ptr<Expr> type_;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, bool rebind, Sym sym, Tok::Tag tag, Ptrs<Ptrn>&& ptrns)
        : Ptrn(loc)
        , rebind_(rebind)
        , sym_(sym)
        , tag_(tag)
        , ptrns_(std::move(ptrns)) {}

    bool rebind() const { return rebind_; }
    Sym sym() const { return sym_; }
    Tok::Tag tag() const { return tag_; }
    const auto& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    // void bind(Scopes&, const Def*, bool rebind = false) const override {}
    // const Def* type(World&, Def2Fields&) const override {}
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    bool rebind_;
    Sym sym_;
    Tok::Tag tag_;
    Ptrs<Ptrn> ptrns_;
};

/*
 * Expr
 */

/// `sym`
class IdExpr : public Expr {
public:
    IdExpr(Loc loc, Sym sym)
        : Expr(loc)
        , sym_(sym) {}
    IdExpr(Tok tok)
        : IdExpr(tok.loc(), tok.sym()) {}

    Sym sym() const { return sym_; }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Sym sym_;
};

/// `tag`
class PrimaryExpr : public Expr {
public:
    PrimaryExpr(Tok tok)
        : Expr(tok.loc())
        , tag_(tok.tag()) {}

    Tok::Tag tag() const { return tag_; }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
};

/// `tag:type`
class LitExpr : public Expr {
public:
    LitExpr(Loc loc, Tok value, Ptr<Expr>&& type)
        : Expr(loc)
        , value_(value)
        , type_(std::move(type)) {}

    Tok value() const { return value_; }
    const Expr* type() const { return type_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok value_;
    Ptr<Expr> type_;
};

class BlockExpr : public Expr {
public:
    BlockExpr(Loc loc, Ptrs<Decl>&& decls, Ptr<Expr>&& expr)
        : Expr(loc)
        , decls_(std::move(decls))
        , expr_(std::move(expr)) {}

    const auto& decls() const { return decls_; }
    const Decl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }
    const Expr* expr() const { return expr_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptrs<Decl> decls_;
    Ptr<Expr> expr_;
};

class TypeExpr : public Expr {
public:
    TypeExpr(Loc loc, Ptr<Expr>&& level)
        : Expr(loc)
        , level_(std::move(level)) {}

    const Expr* level() const { return level_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> level_;
};

// lam

class SimplePiExpr : public Expr {
public:
    SimplePiExpr(Loc loc, Ptr<Expr>&& dom, Ptr<Expr>&& codom)
        : Expr(loc)
        , dom_(std::move(dom))
        , codom_(std::move(codom)) {}

private:
    const Expr* dom() const { return dom_.get(); }
    const Expr* codom() const { return codom_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> dom_;
    Ptr<Expr> codom_;
};

class PiExpr : public Expr {
public:
    class Dom : public Node {
    public:
        Dom(Loc loc, bool implicit, Ptr<Ptrn>&& ptrn)
            : Node(loc)
            , implicit_(implicit)
            , ptrn_(std::move(ptrn)) {}

        bool implicit() const { return implicit_; }
        const Ptrn* ptrn() const { return ptrn_.get(); }
        std::ostream& stream(Tab&, std::ostream&) const override;

    private:
        bool implicit_;
        Ptr<Ptrn> ptrn_;
    };

    PiExpr(Loc loc, Tok::Tag tag, Ptrs<Dom>&& doms, Ptr<Expr>&& codom)
        : Expr(loc)
        , tag_(tag)
        , doms_(std::move(doms))
        , codom_(std::move(codom)) {}

private:
    Tok::Tag tag() const { return tag_; }
    const auto& doms() const { return doms_; }
    const Dom* dom(size_t i) const { return doms_[i].get(); }
    size_t num_doms() const { return doms_.size(); }
    const Expr* codom() const { return codom_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
};

class LamExpr : public Expr {
public:
    class Dom : public PiExpr::Dom {
    public:
        Dom(Loc loc, bool implicit, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& filter)
            : PiExpr::Dom(loc, implicit, std::move(ptrn))
            , filter_(std::move(filter)) {}

        const Expr* filter() const { return filter_.get(); }
        std::ostream& stream(Tab&, std::ostream&) const override;

    private:
        Ptr<Expr> filter_;
    };

    LamExpr(Loc loc, Tok::Tag tag, Sym sym, Ptrs<Dom>&& doms, Ptr<Expr>&& codom, Ptr<Expr>&& body)
        : Expr(loc)
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
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
    Sym sym_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
    Ptr<Expr> body_;
};

class AppExpr : public Expr {
public:
    AppExpr(Loc loc, bool is_explicit, Ptr<Expr>&& callee, Ptr<Expr>&& arg)
        : Expr(loc)
        , is_explicit_(is_explicit)
        , callee_(std::move(callee))
        , arg_(std::move(arg)) {}

    bool is_explicit() const { return is_explicit_; }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    bool is_explicit_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
};

class RetExpr : public Expr {
public:
    RetExpr(Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& callee, Ptr<Expr>&& arg, Ptr<Expr> body)
        : Expr(loc)
        , ptrn_(std::move(ptrn))
        , callee_(std::move(callee))
        , arg_(std::move(arg))
        , body_(std::move(body)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }
    const Expr* body() const { return body_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
    Ptr<Expr> body_;
};
// tuple

class SigmaExpr : public Expr {
public:
    SigmaExpr(Ptr<TuplePtrn>&& ptrn)
        : Expr(ptrn->loc())
        , ptrn_(std::move(ptrn)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<TuplePtrn> ptrn_;
};

class TupleExpr : public Expr {
public:
    TupleExpr(Loc loc, Ptrs<Expr>&& elems)
        : Expr(loc)
        , elems_(std::move(elems)) {}

    const auto& elems() const { return elems_; }
    const Expr* elem(size_t i) const { return elems_[i].get(); }
    size_t num_elems() const { return elems().size(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptrs<Expr> elems_;
};

template<bool arr> class ArrOrPackExpr : public Expr {
public:
    ArrOrPackExpr(Loc loc, Sym sym, Ptr<Expr>&& shape, Ptr<Expr>&& body)
        : Expr(loc)
        , sym_(sym)
        , shape_(std::move(shape))
        , body_(std::move(body)) {}

    Sym sym() const { return sym_; }
    const Expr* shape() const { return shape_.get(); }
    const Expr* body() const { return body_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Sym sym_;
    Ptr<Expr> shape_;
    Ptr<Expr> body_;
};

using ArrExpr  = ArrOrPackExpr<true>;
using PackExpr = ArrOrPackExpr<false>;

class ExtractExpr : public Expr {
public:
    ExtractExpr(Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index)
        : Expr(loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> tuple_;
    Ptr<Expr> index_;
};

class InsertExpr : public Expr {
public:
    InsertExpr(Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index, Ptr<Expr>&& value)
        : Expr(loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index))
        , value_(std::move(value)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }
    const Expr* value() const { return value_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

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
    LetDecl(Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& value)
        : Decl(loc, ptrn->sym())
        , ptrn_(std::move(ptrn))
        , value_(std::move(value)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* value() const { return value_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> value_;
};

class AxiomDecl : public Decl {
public:
    AxiomDecl(Loc loc, Sym sym)
        : Decl(loc, sym) {}
};

class PiDecl : public Decl {
public:
    PiDecl(Loc loc, Sym sym, Ptr<Expr>&& type, Ptr<Expr>&& body)
        : Decl(loc, sym)
        , type_(std::move(type))
        , body_(std::move(body)) {}

    const Expr* type() const { return type_.get(); }
    const Expr* body() const { return body_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> type_;
    Ptr<Expr> body_;
};

class LamDecl : public Decl {
public:
    LamDecl(Ptr<LamExpr>&& lam)
        : Decl(lam->loc(), lam->sym())
        , lam_(std::move(lam)) {}

    const LamExpr* lam() const { return lam_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<LamExpr> lam_;
};

class SigmaDecl : public Decl {
public:
    SigmaDecl(Loc loc, Sym sym, Ptr<Expr>&& type, Ptr<Expr>&& body)
        : Decl(loc, sym)
        , type_(std::move(type))
        , body_(std::move(body)) {}

    const Expr* type() const { return type_.get(); }
    const Expr* body() const { return body_.get(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> type_;
    Ptr<Expr> body_;
};

/*
 * Module
 */

class Module : public Node {
public:
    Module(Loc loc, Ptrs<Decl>&& decls)
        : Node(loc)
        , decls_(std::move(decls)) {}

    const auto& decls() const { return decls_; }
    const Decl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptrs<Decl> decls_;
};

} // namespace ast
} // namespace thorin
