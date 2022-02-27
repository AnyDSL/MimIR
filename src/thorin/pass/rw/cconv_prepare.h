
#ifndef THORIN_PASS_ETA_CONT_H
#define THORIN_PASS_ETA_CONT_H

#include "thorin/pass/pass.h"

namespace thorin {


class EtaExp;

class CConvPrepare : public RWPass<Lam> {
public:
    CConvPrepare(PassMan& man, EtaExp* eta_exp)
        : RWPass(man, "eta_cont")
        , eta_exp_(eta_exp)
        , old2wrapper_(), lam2fscope_()
        , cur_body_(nullptr) {}

    void enter() override;
    const Def* rewrite(const Def*) override;

    Lam* scope(Lam* lam);

    const Def* eta_wrap(const Def* def, CConv cc, const std::string& dbg) {
        auto& w = world();
        auto [entry, inserted] = old2wrapper_.emplace(def, nullptr);
        auto& wrapper = entry->second;
        if (inserted) {
            wrapper = w.nom_lam(def->type()->as<Pi>(), w.dbg(dbg));
            wrapper->app(def, wrapper->var());
            lam2fscope_[wrapper] = scope(curr_nom());
            wrapper_.emplace(wrapper);
        }
        return w.cconv_mark(wrapper, cc);
    }

private:
    EtaExp* eta_exp_;
    DefMap<Lam*> old2wrapper_;
    DefSet wrapper_;
    Lam2Lam lam2fscope_;
    const App* cur_body_;
};

}

#endif
