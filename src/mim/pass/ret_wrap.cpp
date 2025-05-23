#include "mim/pass/ret_wrap.h"

namespace mim {

void RetWrap::enter() {
    auto ret_var = curr_mut()->ret_var();
    if (!ret_var) return;

    // new wrapper that calls the return continuation
    auto ret_cont = world().mut_lam(ret_var->type()->as<Pi>())->set(ret_var->dbg());
    ret_cont->app(false, ret_var, ret_cont->var())->set(ret_var->dbg());

    // rebuild a new "var" that substitutes the actual ret_var with ret_cont
    auto new_vars = curr_mut()->vars();
    assert(new_vars.back() == ret_var && "we assume that the last element is the ret_var");
    new_vars.back() = ret_cont;
    auto new_var    = world().tuple(curr_mut()->dom(), new_vars);
    auto new_defs   = curr_mut()->reduce(new_var);
    curr_mut()->unset()->set(new_defs);
}

} // namespace mim
