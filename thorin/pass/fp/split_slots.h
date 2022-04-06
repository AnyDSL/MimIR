#ifndef THORIN_PASS_FP_SPLIT_SLOTS_H
#define THORIN_PASS_FP_SPLIT_SLOTS_H

#include "thorin/pass/pass.h"

namespace thorin {

class SplitSlots : public FPPass<SplitSlots, Lam> {
public:
    SplitSlots(PassMan& man)
        : FPPass(man, "split_slots") {}

private:
    /// @name PassMan hooks
    ///@{
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Def*) override;
    ///@}
};

} // namespace thorin

#endif
