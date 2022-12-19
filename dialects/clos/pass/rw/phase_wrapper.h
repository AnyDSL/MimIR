#pragma once

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/clos/phase/clos_conv.h"
#include "dialects/clos/phase/lower_typed_clos.h"

namespace thorin {

class ClosConvWrapper : public RWPass<ClosConvWrapper, Lam> {
public:
    ClosConvWrapper(PassMan& man)
        : RWPass(man, "clos_conv") {}

    void prepare() override { clos::ClosConv(world()).run(); }
};

class LowerTypedClosWrapper : public RWPass<LowerTypedClosWrapper, Lam> {
public:
    LowerTypedClosWrapper(PassMan& man)
        : RWPass(man, "lower_typed_clos") {}

    void prepare() override { clos::LowerTypedClos(world()).run(); }
};

}
