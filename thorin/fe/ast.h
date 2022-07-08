#pragma once

#include <deque>
#include <memory>

#include "thorin/fe/tok.h"

namespace thorin {

class Infer;
class Sigma;

namespace fe {

class Scopes;

class AST {
public:
    AST(Loc loc)
        : loc_(loc) {}
    virtual ~AST(){};

    Loc loc() const { return loc_; }

private:
    Loc loc_;
};

/*
 * Pattern
 */

class Ptrn : public AST {
public:
    Ptrn(Loc loc, Sym sym, const Def* type)
        : AST(loc)
        , sym_(sym)
        , type_(type) {}

    Sym sym() const { return sym_; }
    bool is_anonymous() const { return sym_.is_anonymous(); }
    virtual void scrutinize(Scopes&, const Def*) const = 0;
    virtual const Def* type(World&) const = 0;

protected:
    Sym sym_;
    mutable const Def* type_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, Sym sym, const Def* type)
        : Ptrn(loc, sym, type) {}

    void scrutinize(Scopes&, const Def*) const override;
    const Def* type(World&) const override;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, Sym sym, std::deque<std::unique_ptr<Ptrn>>&& ptrns, const Def* type)
        : Ptrn(loc, sym, type)
        , ptrns_(std::move(ptrns)) {}

    const std::deque<std::unique_ptr<Ptrn>>& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void scrutinize(Scopes&, const Def*) const override;
    const Def* type(World&) const override;

private:
    std::deque<std::unique_ptr<Ptrn>> ptrns_;
};

/*
 * Bndr
 */

class Bndr : public AST {
public:
    Bndr(Loc loc, Sym sym)
        : AST(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }
    bool is_anonymous() const { return sym_.is_anonymous(); }
    virtual const Def* type(World&) const          = 0;
    virtual void inject(Scopes&, const Def*) const = 0;

private:
    Sym sym_;
};

class IdBndr : public Bndr {
public:
    IdBndr(Loc loc, Sym sym, const Def* type)
        : Bndr(loc, sym)
        , type_(type) {}

    const Def* type(World&) const override { return type_; }
    void inject(Scopes&, const Def*) const override {}

private:
    const Def* type_;
};

class SigmaBndr : public Bndr {
public:
    SigmaBndr(Loc loc, Sym sym, std::deque<std::unique_ptr<Bndr>>&& bndrs, std::vector<Infer*>&& infers)
        : Bndr(loc, sym)
        , bndrs_(std::move(bndrs))
        , infers_(std::move(infers)) {}

    const std::deque<std::unique_ptr<Bndr>>& bndrs() const { return bndrs_; }
    const Bndr* bndr(size_t i) const { return bndrs_[i].get(); }
    size_t num_bndrs() const { return bndrs().size(); }

    const Def* type(World&) const override;
    void inject(Scopes&, const Def*) const override;

private:
    std::deque<std::unique_ptr<Bndr>> bndrs_;
    std::vector<Infer*> infers_;
    mutable const Def* type_ = nullptr;
};

} // namespace fe
} // namespace thorin
