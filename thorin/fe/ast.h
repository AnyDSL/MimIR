#pragma once

#include <deque>
#include <memory>

#include "thorin/def.h"

#include "thorin/fe/tok.h"

namespace thorin {

class Infer;
class Sigma;
class World;

namespace fe {

class Scopes;
using Def2Fields = DefMap<Array<Sym>>;

/*
 * Pattern
 */

class Ptrn {
public:
    Ptrn(Dbg dbg, bool rebind, const Def* type)
        : dbg_(dbg)
        , rebind_(rebind)
        , type_(type) {}
    virtual ~Ptrn() {}

    Dbg dbg() const { return dbg_; }
    Loc loc() const { return dbg_.loc; }
    Sym sym() const { return dbg_.sym; }
    bool rebind() const { return rebind_; }
    bool is_anonymous() const { return sym() == '_'; }
    virtual void bind(Scopes&, const Def*, bool rebind = false) const = 0;
    virtual const Def* type(World&, Def2Fields&) const                = 0;

protected:
    Dbg dbg_;
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
    TuplePtrn(Dbg dbg, bool rebind, Ptrns&& ptrns, const Def* type, std::vector<Infer*>&& infers, Sigma* sigma)
        : Ptrn(dbg, rebind, type)
        , ptrns_(std::move(ptrns))
        , infers_(std::move(infers))
        , sigma_(sigma) {}

    const Ptrns& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void bind(Scopes&, const Def*, bool rebind = false) const override;
    const Def* type(World&, Def2Fields&) const override;

private:
    Ptrns ptrns_;
    std::vector<Infer*> infers_;
    Sigma* sigma_ = nullptr;
};

} // namespace fe
} // namespace thorin
