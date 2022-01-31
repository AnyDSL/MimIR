#ifndef THORIN_CLOSURE_DESTRUCT_H
#define THORIN_CLOSURE_DESTRUCT_H

#include <set>
#include <functional>

#include "thorin/pass/pass.h"
#include "thorin/pass/fp/eta_exp.h"

namespace thorin {

/// Remove closures for @p Lam's that can be compiled to basic blocks, i.e.
/// are annotated with @p :CA_ret or @p :CA_jmp. 
/// Closure2SSI will preserve closures at control flow branches
/// ClosureDestruct will remove thoes as well.

class Closure2SSI : public FPPass<Closure2SSI, Lam> {
public:
    Closure2SSI(PassMan& man) 
        : Closure2SSI(man, "closure_destruct") 
    {}

    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;

    using Data = LamMap<const Def*>;

protected:
    Closure2SSI(PassMan& man, const std::string& name) 
        : FPPass<Closure2SSI, Lam>(man, name)
        , clos2dropped_(), keep_()
    {}

    const Def* try_drop(const Def*);

    Lam2Lam clos2dropped_;
    Lam2Lam dropped2clos_;
    LamSet keep_;
};

class ClosureDestruct : public Closure2SSI {
public:
    ClosureDestruct(PassMan& man) 
        : Closure2SSI(man, "closure2ssi") 
    {}

    const Def* rewrite(const Def* def);
};



}

#endif
