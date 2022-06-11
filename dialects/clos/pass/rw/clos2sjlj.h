#ifndef THORIN_PASS_SJLJ_H
#define THORIN_PASS_SJLJ_H

#include "thorin/pass/pass.h"

#include "dialects/mem/mem.h"

namespace thorin::clos {

class Clos2SJLJ : public RWPass<Lam> {
public:
    Clos2SJLJ(PassMan& man)
        : RWPass<Lam>(man, "closure2sjlj")
        , lam2tag_()
        , dom2throw_()
        , lam2lpad_()
        , ignore_() {}

    void enter() override;
    const Def* rewrite(const Def*) override;

private:
    const Def* void_ptr() { return mem::type_ptr(world().type_int_width(8)); }
    const Def* jb_type() { return void_ptr(); }
    const Def* rb_type() { return mem::type_ptr(void_ptr()); }
    const Def* tag_type() { return world().type_int_width(32); }

    Lam* get_throw(const Def* res_type);
    Lam* get_lpad(Lam* lam, const Def* rb);

    void get_exn_closures();
    void get_exn_closures(const Def* def, DefSet& visited);

    // clang-format off
    LamMap<std::pair<int, const Def*>> lam2tag_;
    DefMap<Lam*> dom2throw_;
    DefMap<Lam*> lam2lpad_;
    LamSet ignore_;
    // clang-format on

    const Def* cur_rbuf_ = nullptr;
    const Def* cur_jbuf_ = nullptr;
};

} // namespace thorin::clos

#endif
