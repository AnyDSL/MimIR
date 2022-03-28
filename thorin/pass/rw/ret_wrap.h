#ifndef THORIN_PASS_RET_WRAP_H
#define THORIN_PASS_RET_WRAP_H

#include "thorin/pass/pass.h"

namespace thorin {

class RetWrap : public RWPass<Lam> {
public:
    RetWrap(PassMan& man)
        : RWPass(man, "ret_wrap")
        , lam2ret_wrap_() {}

    void enter() override;

private:
    Lam2Lam lam2ret_wrap_;
};

} // namespace thorin

#endif
