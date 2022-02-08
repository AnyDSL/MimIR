#ifndef THORIN_PASS_RW_ZIP_H
#define THORIN_PASS_RW_ZIP_H

#include "thorin/pass/pass.h"

namespace thorin {


class ZipEval : public RWPass<> {
public:
    ZipEval(PassMan& man)
        : RWPass(man, "zip_eval")
    {}
    const Def* rewrite(const Def*) override;
};

}

#endif
