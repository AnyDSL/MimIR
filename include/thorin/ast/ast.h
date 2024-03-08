#pragma once

#include <deque>
#include <memory>

#include "thorin/def.h"

#include "thorin/ast/tok.h"

#include "fe/cast.h"

namespace thorin {

class Infer;
class Sigma;
class World;

using Def2Fields = DefMap<Vector<Sym>>;

namespace ast {

class Scopes;

class Node : public fe::RuntimeCast<Node> {
protected:
    Node(Dbg dbg)
        : dbg_(dbg) {}
    virtual ~Node() {}

public:
    Dbg dbg() const { return dbg_; }
    Loc loc() const { return dbg_.loc; }
    Sym sym() const { return dbg_.sym; }

private:
    Dbg dbg_;
};

/*
 * Pattern
 */

class Ptrn : public Node {
public:
    Ptrn(Dbg dbg, bool rebind, const Def* type)
        : Node(dbg)
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

using Ptrns = std::deque<std::unique_ptr<Ptrn>>;

class IdPtrn : public Ptrn {
public:
    IdPtrn(Dbg dbg, bool rebind, const Def* type)
        : Ptrn(dbg, rebind, type) {}

    void bind(Scopes&, const Def*, bool rebind = false) const override;
    const Def* type(World&, Def2Fields&) const override;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Dbg dbg, bool rebind, Ptrns&& ptrns, const Def* type, std::vector<Infer*>&& infers, Def* decl)
        : Ptrn(dbg, rebind, type)
        , ptrns_(std::move(ptrns))
        , infers_(std::move(infers))
        , decl_(decl) {}

    const Ptrns& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void bind(Scopes&, const Def*, bool rebind = false) const override;
    const Def* type(World&, Def2Fields&) const override;

private:
    Ptrns ptrns_;
    std::vector<Infer*> infers_;
    Def* decl_ = nullptr;
};

/*
 * Expr
 */

#if 0

class Expr : public Node {
protected:
    Expr(Dbg dbg)
        : Node(dbg) {}
};

// lam

class Pi : public Expr {
public:
private:
};

class Lam : public Expr {
public:
    Lam(Dbg dbg, Tok::Tag tag, Ptrns&& domains, Expr* codom, Expr* filter, Expr* body)
        : Expr(dbg)
        , tag_(tag)
        , domains_(std::move(domains))
        , codom_(std::move(codom))
        , filter_(std::move(filter))
        , body_(std::move(body)) {}

    Tok::Tag tag() const { return tag_; }
    const auto& domains() const { return domains_; }
    Expr* codom() const { return codom_; }
    Expr* filter() const { return filter_; }
    Expr* body() const { return body_; }

private:
    Tok::Tag tag_;
    Ptrns domains_;
    Expr* codom_;
    Expr* filter_;
    Expr* body_;
};

class App : public Expr {
public:
    App(Dbg dbg, Expr* callee, Expr* arg)
        : Expr(dbg)
        , callee_(callee)
        , arg_(arg) {}

    Expr* callee() const { return callee_; }
    Expr* arg() const { return arg_; }

private:
    Expr* callee_;
    Expr* arg_;;
};

// tuple

class Sigma : public Expr {
public:
private:
};

class Tuple : public Expr {
public:
private:
};

class Extract : public Expr {
public:
private:
};
#endif

} // namespace ast
} // namespace thorin
