
#ifndef THORIN_PASS_ASSIGN_RET_H
#define THORIN_PASS_ASSIGN_RET_H

#include "thorin/pass/pass.h"
#include "thorin/pass/fp/eta_exp.h"

#include "thorin/transform/closure_conv.h"

namespace thorin {

class ClosureAnalysis : public FPPass<ClosureAnalysis, Lam> {
public:

    ClosureAnalysis(PassMan& man, bool analyze_captrues = true)
        : FPPass<ClosureAnalysis, Lam>(man, "closure_analysis")
        , fva_(analyze_captrues ? std::make_unique<FVA>(man.world()) : nullptr)
        , unknown_(), esc_proc_()
    {}

    using Data = DefMap<CA>;

    void enter() override;
    const Def* rewrite(const Def*) override;
    undo_t analyze(const Def*) override;
    undo_t analyze(const Proxy*) override;

private:

    class Err {
    public:
        Err(Lam* def, undo_t undo)
            : bad_lam(def), undo(undo)
        {}

        operator bool() {
            return bad_lam && undo != No_Undo;
        }

        friend bool operator<(const Err& self, const Err other) {
            return self.undo < other.undo;
        }

    private:
        ClosureAnalysis* pass_;
    public:
        Lam* bad_lam;
        undo_t undo;
    };

    Err ok() { return {nullptr, No_Undo}; }

    Err error(const Def* def);

    const Proxy* proxy(const Def* def, const Err& e) {
        return FPPass::proxy(def->type(), {def, e.bad_lam});
    }

    CA lookup(const Def* def) {
        if (unknown_.contains(def))
            return CA::unknown;
        if (esc_proc_.contains(def))
            return CA::proc_e;
        if (auto v = data().lookup(def))
            return *v;
        return CA::bot;
    }

    Err assign(const DefSet& defs, CA v);
    CA lookup_init(const Def*);

    bool is_escaping(const Def* def) { return ca_is_escaping(lookup_init(def)); }
    bool is_basicblock(const Def* def) { return ca_is_basicblock(lookup_init(def)); }

    const Def* mark(const Def* def);
    bool is_evil(const Def*);

    std::unique_ptr<FVA> fva_;
    DefSet unknown_;
    DefSet esc_proc_;
    bool analyze_captured_ = false;
};

}

#endif
