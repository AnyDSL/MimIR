#include "thorin/plug/clos/clos.h"

#include <thorin/config.h>

#include <thorin/pass/eta_exp.h>
#include <thorin/pass/eta_red.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>
#include <thorin/pass/scalarize.h>

#include "thorin/plug/clos/pass/branch_clos_elim.h"
#include "thorin/plug/clos/pass/clos2sjlj.h"
#include "thorin/plug/clos/pass/clos_conv_prep.h"
#include "thorin/plug/clos/pass/lower_typed_clos_prep.h"
#include "thorin/plug/clos/phase/clos_conv.h"
#include "thorin/plug/clos/phase/lower_typed_clos.h"

using namespace thorin;
using namespace thorin::plug;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"clos", [](Normalizers& normalizers) { clos::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass<clos::clos_conv_prep_pass, clos::ClosConvPrep>(passes, nullptr);
                register_pass<clos::branch_clos_pass, clos::BranchClosElim>(passes);
                register_pass<clos::lower_typed_clos_prep_pass, clos::LowerTypedClosPrep>(passes);
                register_pass<clos::clos2sjlj_pass, clos::Clos2SJLJ>(passes);
                register_phase<clos::clos_conv_phase, clos::ClosConv>(passes);
                register_phase<clos::lower_typed_clos_phase, clos::LowerTypedClos>(passes);
                // TODO:; remove after ho_codegen merge
                passes[flags_t(Annex::Base<clos::eta_red_bool_pass>)] = [&](World&, PipelineBuilder& builder, Ref app) {
                    auto bb      = app->as<App>()->arg();
                    auto bb_only = bb->as<Lit>()->get<u64>();
                    builder.add_pass<EtaRed>(app, bb_only);
                };
            },
            nullptr};
}

namespace thorin::plug::clos {

/*
 * ClosLit
 */

Ref ClosLit::env() {
    assert(def_);
    return std::get<2_u64>(clos_unpack(def_));
}

Ref ClosLit::fnc() {
    assert(def_);
    return std::get<1_u64>(clos_unpack(def_));
}

Lam* ClosLit::fnc_as_lam() {
    auto f = fnc();
    if (auto a = match<attr>(f)) f = a->arg();
    return f->isa_mut<Lam>();
}

Ref ClosLit::env_var() { return fnc_as_lam()->var(Clos_Env_Param); }

ClosLit isa_clos_lit(Ref def, bool lambda_or_branch) {
    auto tpl = def->isa<Tuple>();
    if (tpl && isa_clos_type(def->type())) {
        auto a   = attr::bot;
        auto fnc = std::get<1_u64>(clos_unpack(tpl));
        if (auto fa = match<attr>(fnc)) {
            fnc = fa->arg();
            a   = fa.id();
        }
        if (!lambda_or_branch || fnc->isa<Lam>()) return ClosLit(tpl, a);
    }
    return ClosLit(nullptr, attr::bot);
}

Ref clos_pack(Ref env, Ref lam, Ref ct) {
    assert(env && lam);
    assert(!ct || isa_clos_type(ct));
    auto& w = env->world();
    auto pi = lam->type()->as<Pi>();
    assert(env->type() == pi->dom(Clos_Env_Param));
    ct = (ct) ? ct : clos_type(w.cn(clos_remove_env(pi->dom())));
    return w.tuple(ct, {env->type(), lam, env})->isa<Tuple>();
}

std::tuple<Ref, Ref, Ref> clos_unpack(Ref c) {
    assert(c && isa_clos_type(c->type()));
    // auto& w       = c->world();
    // auto env_type = c->proj(0_u64);
    // // auto pi       = clos_type_to_pi(c->type(), env_type);
    // auto fn       = w.extract(c, w.lit_int(3, 1));
    // auto env      = w.extract(c, w.lit_int(3, 2));
    // return {env_type, fn, env};
    auto [ty, pi, env] = c->projs<3>();
    return {ty, pi, env};
}

Ref clos_apply(Ref closure, Ref args) {
    auto& w           = closure->world();
    auto [_, fn, env] = clos_unpack(closure);
    auto pi           = fn->type()->as<Pi>();
    return w.app(fn, DefVec(pi->num_doms(), [&](auto i) { return clos_insert_env(i, env, args); }));
}

/*
 * closure types
 */

const Sigma* isa_clos_type(Ref def) {
    auto& w  = def->world();
    auto sig = def->isa_mut<Sigma>();
    if (!sig || sig->num_ops() < 3 || sig->op(0_u64) != w.type()) return nullptr;
    auto var = sig->var(0_u64);
    if (sig->op(2_u64) != var) return nullptr;
    auto pi = sig->op(1_u64)->isa<Pi>();
    return (pi && Pi::isa_cn(pi) && pi->num_ops() > 1_u64 && pi->dom(Clos_Env_Param) == var) ? sig : nullptr;
}

Sigma* clos_type(const Pi* pi) { return ctype(pi->world(), pi->doms(), nullptr)->as_mut<Sigma>(); }

const Pi* clos_type_to_pi(Ref ct, Ref new_env_type) {
    assert(isa_clos_type(ct));
    auto& w      = ct->world();
    auto pi      = ct->op(1_u64)->as<Pi>();
    auto new_dom = new_env_type ? clos_sub_env(pi->dom(), new_env_type) : clos_remove_env(pi->dom());
    return w.cn(new_dom);
}

/*
 * closure environments
 */

Ref clos_insert_env(size_t i, Ref env, std::function<Ref(size_t)> f) {
    return (i == Clos_Env_Param) ? env : f(shift_env(i));
}

Ref clos_remove_env(size_t i, std::function<Ref(size_t)> f) { return f(skip_env(i)); }

Ref ctype(World& w, Defs doms, Ref env_type) {
    if (!env_type) {
        auto sigma = w.mut_sigma(w.type(), 3_u64)->set("Clos");
        sigma->set(0_u64, w.type());
        sigma->set(1_u64, ctype(w, doms, sigma->var(0_u64)));
        sigma->set(2_u64, sigma->var(0_u64));
        return sigma;
    }
    return w.cn(
        DefVec(doms.size() + 1, [&](auto i) { return clos_insert_env(i, env_type, [&](auto j) { return doms[j]; }); }));
}

} // namespace thorin::plug::clos
