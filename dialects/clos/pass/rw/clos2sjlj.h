#pragma once

#include "thorin/pass/pass.h"

#include "dialects/clos/clos.h"
#include "dialects/mem/mem.h"

namespace thorin::clos {

class Clos2SJLJ : public RWPass<Clos2SJLJ, Lam> {
public:
    Clos2SJLJ(PassMan& man)
        : RWPass(man, "closure2sjlj")
        , lam2tag_()
        , dom2throw_()
        , lam2lpad_()
        , ignore_() {}

    void enter() override;
    Ref rewrite(Ref) override;

private:
    Ref void_ptr() { return world().annex<clos::BufPtr>(); }
    Ref jb_type() { return void_ptr(); }
    Ref rb_type() { return world().call<mem::Ptr0>(void_ptr()); }
    Ref tag_type() { return world().type_int(32); }

    Lam* get_throw(Ref res_type);
    Lam* get_lpad(Lam* lam, Ref rb);

    void get_exn_closures();
    void get_exn_closures(Ref def, DefSet& visited);

    // clang-format off
    LamMap<std::pair<int, Ref>> lam2tag_;
    DefMap<Lam*> dom2throw_;
    DefMap<Lam*> lam2lpad_;
    LamSet ignore_;
    // clang-format on

    Ref cur_rbuf_ = nullptr;
    Ref cur_jbuf_ = nullptr;
};

} // namespace thorin::clos
