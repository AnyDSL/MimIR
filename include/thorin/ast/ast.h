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

namespace ir = thorin;

template<class T> using Ptr  = fe::Arena::Ptr<const T>;
template<class T> using Ptrs = std::deque<Ptr<T>>;

class AST {
public:
    AST(World& world)
        : world_(world) {}

    World& world() { return world_; }
    Driver& driver();

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
};

class Scopes;

class Node : public fe::RuntimeCast<Node> {
protected:
    Node(AST& ast, Dbg dbg)
        : ast_(ast)
        , dbg_(dbg) {}
    virtual ~Node() {}

public:
    AST& ast() const { return ast_; }
    Dbg dbg() const { return dbg_; }
    Loc loc() const { return dbg_.loc; }
    Sym sym() const { return dbg_.sym; }

private:
    AST& ast_;
    Dbg dbg_;
};

/*
 * Pattern
 */

class Ptrn : public Node {
public:
    Ptrn(AST& ast, Dbg dbg, bool rebind, const Def* type)
        : Node(ast, dbg)
        , rebind_(rebind)
        , type_(type) {}
    virtual ~Ptrn() {}

    bool rebind() const { return rebind_; }
    bool is_anonymous() const { return sym() == '_'; }
    virtual void bind(Scopes&, const Def*, bool rebind = false) const = 0;
    virtual const Def* type(World&, Def2Fields&) const                = 0;

protected:
    bool rebind_;
    mutable const Def* type_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(AST& ast, Dbg dbg, bool rebind, const Def* type)
        : Ptrn(ast, dbg, rebind, type) {}

    void bind(Scopes&, const Def*, bool rebind = false) const override;
    const Def* type(World&, Def2Fields&) const override;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(AST& ast,
              Dbg dbg,
              bool rebind,
              Ptrs<Ptrn>&& ptrns,
              const Def* type,
              std::vector<Infer*>&& infers,
              Def* decl)
        : Ptrn(ast, dbg, rebind, type)
        , ptrns_(std::move(ptrns))
        , infers_(std::move(infers))
        , decl_(decl) {}

    const auto& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void bind(Scopes&, const Def*, bool rebind = false) const override;
    const Def* type(World&, Def2Fields&) const override;

private:
    Ptrs<Ptrn> ptrns_;
    std::vector<Infer*> infers_;
    Def* decl_ = nullptr;
};

/*
 * Expr
 */

class Expr : public Node {
protected:
    Expr(AST& ast, Dbg dbg)
        : Node(ast, dbg) {}
};

class IdExpr : public Node {
protected:
    IdExpr(AST& ast, Dbg dbg, Sym)
        : Node(ast, dbg) {}

public:
    Sym sym() const { return sym_; }

private:
    Sym sym_;
};

class Decl : public Expr {
protected:
    Decl(AST& ast, Dbg dbg)
        : Expr(ast, dbg) {}
};

class Let : public Decl {
public:
    Let(AST& ast, Dbg dbg, Ptr<Expr>&& type, Ptr<Expr>&& init)
        : Decl(ast, dbg)
        , type_(std::move(type))
        , init_(std::move(init)) {}

    const Expr* type() const { return type_.get(); }
    const Expr* init() const { return init_.get(); }

private:
    Ptr<Expr> type_;
    Ptr<Expr> init_;
};

class Axiom : public Decl {};

// lam

class Pi : public Decl {
public:
    Pi(AST& ast, Dbg dbg, Tok::Tag tag, Ptrs<Ptrn>&& domains, Ptr<Expr>&& codom)
        : Decl(ast, dbg)
        , tag_(tag)
        , domains_(std::move(domains))
        , codom_(std::move(codom)) {}

private:
    Tok::Tag tag() const { return tag_; }
    const auto& domains() const { return domains_; }
    const Expr* codom() const { return codom_.get(); }

private:
    Tok::Tag tag_;
    Ptrs<Ptrn> domains_;
    Ptr<Expr> codom_;
};

class Lam : public Decl {
public:
    Lam(AST& ast, Dbg dbg, Tok::Tag tag, Ptrs<Ptrn>&& domains, Ptr<Expr>&& codom, Ptr<Expr>&& filter, Ptr<Expr>&& body)
        : Decl(ast, dbg)
        , tag_(tag)
        , domains_(std::move(domains))
        , codom_(std::move(codom))
        , filter_(std::move(filter))
        , body_(std::move(body)) {}

    Tok::Tag tag() const { return tag_; }
    const auto& domains() const { return domains_; }
    const Expr* codom() const { return codom_.get(); }
    const Expr* filter() const { return filter_.get(); }
    const Expr* body() const { return body_.get(); }

private:
    Tok::Tag tag_;
    Ptrs<Ptrn> domains_;
    Ptr<Expr> codom_;
    Ptr<Expr> filter_;
    Ptr<Expr> body_;
};

class App : public Expr {
public:
    App(AST& ast, Dbg dbg, Ptr<Expr>&& callee, Ptr<Expr>&& arg)
        : Expr(ast, dbg)
        , callee_(std::move(callee))
        , arg_(std::move(arg)) {}

    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }

private:
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
};

// tuple

class Sigma : public Decl {
public:
    Sigma(AST& ast, Dbg dbg, Ptrs<Ptrn>&& elems)
        : Decl(ast, dbg)
        , elems_(std::move(elems)) {}

    const auto& elems() const { return elems_; }
    const Ptrn* elem(size_t i) const { return elems_[i].get(); }
    size_t num_elems() const { return elems().size(); }

private:
    Ptrs<Ptrn> elems_;
};

class Tuple : public Expr {
public:
    Tuple(AST& ast, Dbg dbg, Ptrs<Expr>&& elems)
        : Expr(ast, dbg)
        , elems_(std::move(elems)) {}

    const auto& elems() const { return elems_; }
    const Expr* elem(size_t i) const { return elems_[i].get(); }
    size_t num_elems() const { return elems().size(); }

private:
    Ptrs<Expr> elems_;
};

class Extract : public Expr {
public:
    Extract(AST& ast, Dbg dbg, Ptr<Expr>&& tuple, Ptr<Expr>&& index)
        : Expr(ast, dbg)
        , tuple_(std::move(tuple))
        , index_(std::move(index)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }

private:
    Ptr<Expr> tuple_;
    Ptr<Expr> index_;
};

class Insert : public Expr {
public:
    Insert(AST& ast, Dbg dbg, Ptr<Expr>&& tuple, Ptr<Expr>&& index, Ptr<Expr>&& value)
        : Expr(ast, dbg)
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
 * Module
 */

class Module : public Node {
public:
    Module(AST& ast, Dbg dbg, Ptrs<Decl>&& decls)
        : Node(ast, dbg)
        , decls_(std::move(decls)) {}

    const auto& decls() const { return decls_; }
    const Decl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }

private:
    Ptrs<Decl> decls_;
};

} // namespace ast
} // namespace thorin
