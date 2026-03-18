#include "mim/plug/aie2p/phase/lower_aie2p.h"

#include <cassert>

#include <mim/lam.h>

#include "mim/plug/aie2p/autogen.h"
#include "mim/plug/direct/direct.h"

namespace mim::plug::aie2p::phase {

const LowerAIE2P::IntrinsicMap LowerAIE2P::intrinsic_names_ = {
    { Annex::Base<get_coreid>,                 "llvm.aie2p.get.coreid"},
    {        Annex::Base<clb>,                        "llvm.aie2p.clb"},
    { Annex::Base<srs_i16_32>,         "llvm.aie2p.I512.v32.acc64.srs"},
    {Annex::Base<mac_i16_i64>, "llvm.aie2p.I512.I512.ACC1024.mac.conf"},
    { Annex::Base<srs_i32_16>,         "llvm.aie2p.I512.v16.acc64.srs"},
    {  Annex::Base<mac_4x4x8>, "llvm.aie2p.I512.I512.ACC2048.mac.conf"},
};

const Def*
LowerAIE2P::lower_to_cps_intrinsic(const Def* arg_rewritten, const Def* dom, const Def* ret, const char* llvm_name) {
    auto& new_w  = new_world();
    auto& cached = wrapped_cache_[reinterpret_cast<flags_t>(llvm_name)];

    if (!cached) {
        auto cn_ret = new_w.cn(ret);

        if (auto sigma = dom->isa<Sigma>(); sigma && sigma->num_ops() >= 2) {
            // Multi-arg intrinsic: create flat LLVM-facing lambda + forwarding lambda.
            DefVec flat_doms;
            for (auto op : sigma->ops())
                flat_doms.push_back(op);
            flat_doms.push_back(cn_ret);
            auto flat_lam = new_w.mut_con(flat_doms)->set(llvm_name);

            // Forwarding lambda with 2-element domain for cps2ds_dep compatibility.
            auto fwd_lam   = new_w.mut_con({dom, cn_ret})->set("fwd");
            auto sigma_var = fwd_lam->var(2, 0);
            auto cont_var  = fwd_lam->var(2, 1);
            DefVec call_args;
            for (size_t i = 0; i < sigma->num_ops(); ++i)
                call_args.push_back(sigma_var->proj(sigma->num_ops(), i));
            call_args.push_back(cont_var);
            fwd_lam->app(true, flat_lam, call_args);

            cached = direct::op_cps2ds_dep(fwd_lam);
        } else {
            // Single-arg / unit intrinsic: 2-element domain works directly.
            auto cps_lam = new_w.mut_con({dom, cn_ret})->set(llvm_name);
            cached       = direct::op_cps2ds_dep(cps_lam);
        }
    }
    return new_w.app(cached, arg_rewritten);
}

const Def* LowerAIE2P::rewrite_imm_App(const App* app) {
    if (is_bootstrapping()) return Rewriter::rewrite_imm_App(app);

    auto arg_rewritten = rewrite(app->arg());
    if (!arg_rewritten) return Rewriter::rewrite_imm_App(app);

    auto [axm, curry, _] = Axm::get(app);
    if (!axm || curry != 0) return Rewriter::rewrite_imm_App(app);

    auto it = intrinsic_names_.find(axm->base());
    if (it == intrinsic_names_.end()) return Rewriter::rewrite_imm_App(app);

    auto dom = rewrite(app->arg()->type());
    auto ret = rewrite(app->type());
    if (!dom || !ret) return Rewriter::rewrite_imm_App(app);

    return lower_to_cps_intrinsic(arg_rewritten, dom, ret, it->second);
}

} // namespace mim::plug::aie2p::phase
