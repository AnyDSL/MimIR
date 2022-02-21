#ifndef THORIN_PASS_RW_LAM_SPEC_H
#define THORIN_PASS_RW_LAM_SPEC_H

#include "thorin/pass/pass.h"

namespace thorin {

class LamSpec : public RWPass<Lam> {
public:
    LamSpec(PassMan& man)
        : RWPass(man, "lam_spec") {}

    using Data = std::tuple<>;

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    ///@}

    Def2Def old2new_;
};

} // namespace thorin

#endif
