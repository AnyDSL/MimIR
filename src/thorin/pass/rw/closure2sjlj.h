
#ifndef THORIN_PASS_SJLJ_H
#define THORIN_PASS_SJLJ_H

#include "thorin/pass/pass.h"

namespace thorin {

class Closure2SjLj : public RWPass<Lam> {
public:
    Closure2SjLj(PassMan& man)
        : RWPass<Lam>(man, "closure2sjlj")
        , lam2tag_(), dom2throw_(), lam2lpad_() {}

    void enter() override;
    const Def* rewrite(const Def*) override;

private:

    const Def* void_ptr() { return world().type_ptr(world().type_int_width(8)); }
    const Def* jb_type() { return void_ptr(); }
    const Def* rb_type() { return world().type_ptr(void_ptr()); }
    const Def* tag_type() { return world().type_int(); }

    LamMap<std::pair<int, const Def*>> lam2tag_;
    DefMap<Lam*> dom2throw_;
    LamMap<Lam*> lam2lpad_;

    Lam* get_throw(const Def* res_type);
    Lam* get_lpad(Lam* lam);
    void get_exn_closures();

    const Def* cur_rbuf_ = nullptr;
    const Def* cur_jbuf_ = nullptr;

#if 0
    cn [ :mem, [i8*, i8*, i64], <type> ]
        fn (:mem, [buf, res, tag], x) =
            m2 := store res x;
            :longjmp (m2, buf, tag)

    
    cn [:mem, <env_type>, i8*] ~> ctype [:mem, i8*]
    fn (m, env, res_ptr) :=
        args := load ptr
        f (m, env, args...)

    cn [:mem, <args>, i8*] ~> CTYPE[:mem, i8*]
        <body> <args>
#endif

    

};

}

#endif
