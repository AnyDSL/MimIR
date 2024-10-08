#pragma once

#include <mim/pass/pass.h>
#include <mim/plug/mem/mem.h>

#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

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
    Ref tag_type() { return world().type_i32(); }

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

} // namespace mim::plug::clos
