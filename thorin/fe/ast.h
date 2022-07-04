#pragma once

#include <deque>
#include <memory>

#include "thorin/fe/tok.h"

namespace thorin::fe {

class Scopes;

class AST {
public:
    AST(Loc loc)
        : loc_(loc) {}
    virtual ~AST() {};

    Loc loc() const { return loc_; }

private:
    Loc loc_;
};

/*
 * Ptrn
 */

class Ptrn : public AST {
public:
    Ptrn(Loc loc, const Def* type)
        : AST(loc)
        , type_(type) {}

    virtual void scrutinize(Scopes&, const Def*) const = 0;

private:
    const Def* type_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, Sym sym, const Def* type)
        : Ptrn(loc, type)
        , sym_(sym) {}

    void scrutinize(Scopes&, const Def*) const override;

private:
    Sym sym_;
    const Def* type_;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, std::deque<std::unique_ptr<Ptrn>>&& ptrns, const Def* type)
        : Ptrn(loc, type)
        , ptrns_(std::move(ptrns)) {}

    void scrutinize(Scopes&, const Def*) const override;

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

    //virtual void scrutinize(Scopes&, const Def*) const = 0;

private:
    Sym sym_;
};

class IdBndr : public Bndr {
public:
    IdBndr(Loc loc, Sym sym, const Def* type)
        : Bndr(loc, sym)
        , type_(type) {}

    //void scrutinize(Scopes&, const Def*) const override;

private:
    const Def* type_;
};

class SigmaBndr : public Bndr {
public:
    SigmaBndr(Loc loc, Sym sym, std::deque<std::unique_ptr<Bndr>>&& bndrs)
        : Bndr(loc, sym)
        , bndrs_(std::move(bndrs)) {}

    //void scrutinize(Scopes&, const Def*) const override;

private:
    std::deque<std::unique_ptr<Bndr>> bndrs_;
};

} // namespace thorin::fe
