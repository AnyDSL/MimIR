#pragma once

#include <deque>
#include <memory>

#include "thorin/fe/tok.h"

namespace thorin {

class Infer;
class Sigma;
class World;

namespace fe {

class Scopes;

class AST {
public:
    AST(Loc loc)
        : loc_(loc) {}
    virtual ~AST() {}

    Loc loc() const { return loc_; }

private:
    Loc loc_;
};

/*
 * Pattern
 */

class Ptrn : public AST {
public:
    Ptrn(Loc loc, Sym sym, bool rebind, const Def* type)
        : AST(loc)
        , rebind_(rebind)
        , sym_(sym)
        , type_(type) {}

    bool rebind() const { return rebind_; }
    Sym sym() const { return sym_; }
    bool is_anonymous() const { return sym_.is_anonymous(); }
    virtual void bind(Scopes&, const Def*) const = 0;
    virtual const Def* type(World&) const        = 0;
    const Def* dbg(World&);

protected:
    bool rebind_;
    Sym sym_;
    Loc loc_;
    mutable const Def* type_;
};

using Ptrns = std::deque<std::unique_ptr<Ptrn>>;

class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, Sym sym, bool rebind, const Def* type)
        : Ptrn(loc, sym, rebind, type) {}

    void bind(Scopes&, const Def*) const override;
    const Def* type(World&) const override;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, Sym sym, bool rebind, Ptrns&& ptrns, const Def* type, std::vector<Infer*>&& infers)
        : Ptrn(loc, sym, rebind, type)
        , ptrns_(std::move(ptrns))
        , infers_(std::move(infers)) {}

    const Ptrns& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void bind(Scopes&, const Def*) const override;
    const Def* type(World&) const override;

private:
    Ptrns ptrns_;
    std::vector<Infer*> infers_;
};

} // namespace fe
} // namespace thorin
