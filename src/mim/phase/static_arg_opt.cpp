#include "mim/phase/static_arg_opt.h"

namespace mim {

bool StaticArgOpt::analyze() {
    for (auto def : old_world().annexes())
        visit(def);
    for (auto def : old_world().externals().muts())
        visit(def);

    return false; // no fixed-point neccessary
}

void StaticArgOpt::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;

    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>(); lam && lam->has_var()) {
            uint64_t statics = 0;
            for (size_t i = 0, e = lam->num_tvars(); i != e; ++i, statics >>= 1)
                statics |= uint64_t(lam->tvar(i) == app->targ(i)) << e;

            if (statics) {
                if (auto [i, ins] = lam2statics_.emplace(lam, statics); !ins) i->second &= statics;
            }
        }
    }

    for (auto d : def->deps())
        visit(d);
}

const Def* StaticArgOpt::rewrite_imm_App(const App* app) {
    if (auto old_lam = app->callee()->isa_mut<Lam>(); old_lam) {
        if (auto i = lam2statics_.find(old_lam); i != lam2statics_.end()) {
            if (auto statics = i->second) {
                Lam* p; // pre-header
                Lam* h; // header
                if (auto i = lam2ph_.find(old_lam); i != lam2ph_.end()) {
                    std::tie(p, h) = i->second;
                } else {
                    auto h_doms = DefVec();
                    auto s      = statics;
                    for (size_t i = 0, e = old_lam->num_tvars(); i != e; ++i, s <<= 1)
                        if (!(s & 1)) h_doms.emplace_back(rewrite(old_lam->tdom(i)));

                    p = new_world().mut_lam(rewrite(old_lam->type())->as<Pi>());
                    h = new_world().mut_lam(h_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());

                    auto new_vars = DefVec();
                    auto new_args = DefVec();
                    s             = statics;
                    for (size_t i = 0, j = 0, e = old_lam->num_tvars(); i != e; ++i, s <<= 1) {
                        if (s & 1)
                            new_vars.emplace_back(p->tvar(i));
                        else {
                            new_vars.emplace_back(h->var(h_doms.size(), j++));
                            new_args.emplace_back(p->tvar(i));
                        }
                    }

                    // TODO properly rewrite filter
                    p->app(false, h, new_args);
                    map(old_lam->var(), new_vars);
                }
            }
        }
    }

    DLOG("beta-reduce: `{}`", old_lam);
    if (auto var = old_lam->has_var()) {
        auto new_arg = rewrite(app->arg());
        map(var, new_arg);
        // if we want to reduce more than once, we need to push/pop
    }
    todo_ = true;
    return rewrite(old_lam->body());
}

return Rewriter::rewrite_imm_App(app);
}

} // namespace mim
