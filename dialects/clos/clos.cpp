#include "dialects/clos/clos.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/clos/pass/fp/lower_typed_clos_prep.h"
#include "dialects/clos/pass/rw/branch_clos_elim.h"
#include "dialects/clos/pass/rw/clos2sjlj.h"
#include "dialects/clos/pass/rw/clos_conv_prep.h"
#include "dialects/clos/pass/rw/phase_wrapper.h"
#include "dialects/mem/mem.h"
#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/rw/reshape.h"
#include "dialects/mem/phases/rw/add_mem.h"
#include "dialects/refly/passes/debug_dump.h"

namespace thorin::clos {

/*
 * ClosLit
 */

const Def* ClosLit::env() {
    assert(def_);
    return std::get<2_u64>(clos_unpack(def_));
}

const Def* ClosLit::fnc() {
    assert(def_);
    return std::get<1_u64>(clos_unpack(def_));
}

Lam* ClosLit::fnc_as_lam() {
    auto f = fnc();
    if (auto a = match<attr>(f)) f = a->arg();
    return f->isa_nom<Lam>();
}

const Def* ClosLit::env_var() { return fnc_as_lam()->var(Clos_Env_Param); }

ClosLit isa_clos_lit(const Def* def, bool lambda_or_branch) {
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

const Def* clos_pack_dbg(const Def* env, const Def* lam, const Def* dbg, const Def* ct) {
    assert(env && lam);
    assert(!ct || isa_clos_type(ct));
    auto& w = env->world();
    auto pi = lam->type()->as<Pi>();
    assert(env->type() == pi->dom(Clos_Env_Param));
    ct = (ct) ? ct : clos_type(w.cn(clos_remove_env(pi->dom())));
    return w.tuple(ct, {env->type(), lam, env}, dbg)->isa<Tuple>();
}

std::tuple<const Def*, const Def*, const Def*> clos_unpack(const Def* c) {
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

const Def* clos_apply(const Def* closure, const Def* args) {
    auto& w           = closure->world();
    auto [_, fn, env] = clos_unpack(closure);
    auto pi           = fn->type()->as<Pi>();
    return w.app(fn, DefArray(pi->num_doms(), [&](auto i) { return clos_insert_env(i, env, args); }));
}

/*
 * closure types
 */

const Sigma* isa_clos_type(const Def* def) {
    auto& w  = def->world();
    auto sig = def->isa_nom<Sigma>();
    if (!sig || sig->num_ops() < 3 || sig->op(0_u64) != w.type()) return nullptr;
    auto var = sig->var(0_u64);
    if (sig->op(2_u64) != var) return nullptr;
    auto pi = sig->op(1_u64)->isa<Pi>();
    return (pi && pi->is_cn() && pi->num_ops() > 1_u64 && pi->dom(Clos_Env_Param) == var) ? sig : nullptr;
}

Sigma* clos_type(const Pi* pi) { return ctype(pi->world(), pi->doms(), nullptr)->as_nom<Sigma>(); }

const Pi* clos_type_to_pi(const Def* ct, const Def* new_env_type) {
    assert(isa_clos_type(ct));
    auto& w      = ct->world();
    auto pi      = ct->op(1_u64)->as<Pi>();
    auto new_dom = new_env_type ? clos_sub_env(pi->dom(), new_env_type) : clos_remove_env(pi->dom());
    return w.cn(new_dom);
}

/*
 * closure environments
 */

const Def* clos_insert_env(size_t i, const Def* env, std::function<const Def*(size_t)> f) {
    return (i == Clos_Env_Param) ? env : f(shift_env(i));
}

const Def* clos_remove_env(size_t i, std::function<const Def*(size_t)> f) { return f(skip_env(i)); }

const Def* ctype(World& w, Defs doms, const Def* env_type) {
    if (!env_type) {
        auto sigma = w.nom_sigma(w.type(), 3_u64, w.dbg("Clos"));
        sigma->set(0_u64, w.type());
        sigma->set(1_u64, ctype(w, doms, sigma->var(0_u64)));
        sigma->set(2_u64, sigma->var(0_u64));
        return sigma;
    }
    return w.cn(DefArray(doms.size() + 1,
                         [&](auto i) { return clos_insert_env(i, env_type, [&](auto j) { return doms[j]; }); }));
}

} // namespace thorin::clos

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"clos",
            [](PipelineBuilder& builder) {
                int base = 140;
                // closure_conv

                builder.add_opt(base++);

                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<DebugDump>(); });
                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<mem::Reshape>(mem::Reshape::Flat); });
                // or codegen prep phase
                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<mem::AddMemWrapper>(); });
                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<DebugDump>(); });

                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<clos::ClosConvPrep>(nullptr); });
                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<EtaExp>(nullptr); });
                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<ClosConvWrapper>(); });

                builder.extend_opt_phase(base++, [](PassMan& man) {
                    auto er = man.add<EtaRed>(true);
                    auto ee = man.add<EtaExp>(er);
                    man.add<Scalerize>(ee);
                });
                // lower_closures
                builder.extend_opt_phase(base++, [](PassMan& man) {
                    man.add<Scalerize>(nullptr);
                    man.add<clos::BranchClosElim>();
                    man.add<mem::CopyProp>(nullptr, nullptr, true);
                    man.add<clos::LowerTypedClosPrep>();
                    man.add<clos::Clos2SJLJ>();
                });

                builder.extend_opt_phase(base++, [](PassMan& man) { man.add<LowerTypedClosWrapper>(); });

                // builder.add_opt(base++);
            },
            nullptr, [](Normalizers& normalizers) { clos::register_normalizers(normalizers); }};
}
