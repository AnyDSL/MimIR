#pragma once

#include <deque>
#include <memory>

#include "thorin/fe/tok.h"

namespace thorin::fe {

class Binder;

class Ptrn {
public:
    Ptrn(Loc loc)
        : loc_(loc) {}
    virtual ~Ptrn() {}

    virtual void scrutinize(Binder&, const Def*) const = 0;

private:
    Loc loc_;
};

class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, Sym sym, const Def* type)
        : Ptrn(loc)
        , sym_(sym)
        , type_(type) {}

    void scrutinize(Binder&, const Def*) const override;

private:
    Sym sym_;
    const Def* type_;
};

class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, std::deque<std::unique_ptr<Ptrn>>&& ptrns)
        : Ptrn(loc)
        , ptrns_(std::move(ptrns)) {}

    void scrutinize(Binder&, const Def*) const override;

private:
    std::deque<std::unique_ptr<Ptrn>> ptrns_;
};

} // namespace thorin::fe
