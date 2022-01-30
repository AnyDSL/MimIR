#ifndef THORIN_CLOSURE_DESTRUCT_H
#define THORIN_CLOSURE_DESTRUCT_H

#include <set>
#include <functional>

#include "thorin/pass/pass.h"
#include "thorin/pass/fp/eta_exp.h"

namespace thorin {

// class PTG;

class ClosureDestruct : public FPPass<ClosureDestruct, Lam> {
public:
    ClosureDestruct(PassMan& man) 
        : ClosureDestruct(man, "closure_destruct") 
    {}

    const Def* rewrite(const Def*) override;
    undo_t analyze(const Proxy*) override;

    using Data = LamMap<const Def*>;

protected:
    ClosureDestruct(PassMan& man, const std::string& name) 
        : FPPass<ClosureDestruct, Lam>(man, name)
        , clos2dropped_(), keep_()
    {}

    Lam2Lam clos2dropped_;
    Lam2Lam dropped2clos_;
    LamSet keep_;
};

class Closure2SSI : public ClosureDestruct {
public:
    Closure2SSI(PassMan& man) 
        : ClosureDestruct(man, "closure2ssi") 
    {}

    undo_t analyze(const Def* def) override;
};



}

#endif
