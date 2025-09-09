#pragma once

#include <mim/pass.h>

#include <mim/plug/mem/mem.h>

#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

class Clos2SJLJ : public RWPass<Clos2SJLJ, Lam> {
public:
    Clos2SJLJ(World& world, flags_t annex)
        : RWPass(world, annex) {}
    Clos2SJLJ* recreate() final { return driver().stage<Clos2SJLJ>(world(), annex()); }

    void enter() override;
    const Def* rewrite(const Def*) override;

private:
    const Def* void_ptr() { return world().annex<clos::BufPtr>(); }
    const Def* jb_type() { return void_ptr(); }
    const Def* rb_type() { return world().call<mem::Ptr0>(void_ptr()); }
    const Def* tag_type() { return world().type_i32(); }

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

} // namespace mim::plug::clos
