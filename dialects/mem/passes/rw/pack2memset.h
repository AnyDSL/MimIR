#pragma once

#include "thorin/pass/pass.h"

namespace thorin::mem {

class Pack2Memset : public RWPass<Pack2Memset, Lam> {
public:
    Pack2Memset(PassMan& man)
        : RWPass(man, "pack2memset") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::mem
