#ifndef THORIN_PASS_RW_ZIP_H
#define THORIN_PASS_RW_ZIP_H

#include "thorin/pass/pass.h"

namespace thorin {


class ZipEval : public FPPass<ZipEval,Lam> {
public:
    ZipEval(PassMan& man)
        : FPPass(man, "zip_eval")
    {}
    const Def* rewrite(const Def*) override;

    enum Lattice : bool { Callee, Non_Callee_1 };
    static std::string_view lattice2str(Lattice l) { return l == Callee ? "Callee" : "Non_Callee_1"; }

    using Data = LamMap<Lattice>;

private:
    undo_t analyze(const Def*) override;
//    LamMap<std::pair<Lam*, Lam*>> replacements;
    DefSet ignore;
    Def2Def replace; // zip, lam
};

}

#endif
