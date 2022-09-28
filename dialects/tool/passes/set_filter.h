#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::tool {

class SetFilter : public RWPass<SetFilter, Lam> {
public:
    SetFilter(PassMan& man)
        : RWPass(man, "set_filter") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::tool
