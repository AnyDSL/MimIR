#pragma once

#include "thorin/pass/pass.h"

namespace thorin::autodiff {

class AutodiffPrep : public RWPass<AutodiffPrep, Lam> {
public:
    AutodiffPrep(PassMan& man)
        : RWPass(man, "autodiff_prep") {}

    const Def* rewrite(const Def*) override;

    void mark(const Def* def){
        marked.insert(def);
    }

    DefSet visited;
    DefSet marked;
};

} // namespace thorin::autodiff
