#include "mim/phase/static_arg_opt.h"

namespace mim {

bool StaticArgOpt::analyze() {
    for (auto def : old_world().annexes())
        analyze(def);
    for (auto def : old_world().externals().muts())
        analyze(def);

    return false; // no fixed-point neccessary
}

void StaticArgOpt::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;

    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>(); lam && lam->has_var()) {
            auto n           = lam->num_tvars();
            auto statics     = Vector<bool>(n, false);
            bool has_statics = false;
            for (size_t i = 0; i != n; ++i)
                if (lam->tvar(i) == app->targ(i)) {
                    statics[i]  = true;
                    has_statics = true;
                }

            if (has_statics) {
                if (auto [i, ins] = lam2statics_.emplace(lam, statics); !ins) {
                    auto& cur = i->second;
                    for (size_t i = 0; i != n; ++i)
                        cur[i] &= statics[i];
                }
            }
        }
    }

    for (auto d : def->deps())
        analyze(d);
}

const Def* StaticArgOpt::rewrite_mut_Lam(Lam* old_lam) {
    if (auto i = lam2statics_.find(old_lam); i != lam2statics_.end()) {
        auto statics   = i->second;
        auto n         = statics.size();
        auto loop_doms = DefVec();
        for (size_t i = 0; i != n; ++i)
            if (!statics[i]) loop_doms.emplace_back(rewrite(old_lam->tdom(i)));

        if (auto num_loop_doms = loop_doms.size(); num_loop_doms > 0) {
            todo_        = true;
            auto new_lam = new_world().mut_lam(rewrite(old_lam->type())->as<Pi>())->set(old_lam->dbg());
            auto loop    = new_world().mut_lam(loop_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());
            loop->debug_suffix("_loop");

            auto vars = DefVec();
            auto args = DefVec();
            for (size_t i = 0, j = 0; i != n; ++i) {
                if (statics[i])
                    vars.emplace_back(new_lam->var(n, i));
                else {
                    vars.emplace_back(loop->var(loop_doms.size(), j++));
                    args.emplace_back(new_lam->var(n, i));
                }
            }

            map(old_lam, new_lam);
            map(old_lam->var(), vars);
            new_lam->app(false, loop, args); // TODO properly rewrite filter
            loop->set(rewrite(old_lam->filter()), rewrite(old_lam->body()));

            return new_lam;
        }
    }

    return RWPhase::rewrite_mut_Lam(old_lam);
}

const Def* StaticArgOpt::rewrite_imm_App(const App* old_app) {
    if (auto old_lam = old_app->callee()->isa_mut<Lam>(); old_lam) {
        if (auto i = lam2statics_.find(old_lam); i != lam2statics_.end()) {
            if (auto statics = i->second; std::ranges::any_of(statics, [](bool b) { return b; })) {
                auto new_lam  = rewrite(old_lam)->as_mut<Lam>();
                auto loop     = new_lam->body()->as<App>()->callee()->as_mut<Lam>();
                auto n        = statics.size();
                auto new_args = DefVec();

                for (size_t i = 0; i != n; ++i) {
                    auto old_arg = old_app->arg(n, i);
                    if (!statics[i] || old_arg != old_lam->var(n, i)) new_args.emplace_back(rewrite(old_arg));
                }

                if (new_args.size() != n) return new_world().app(loop, new_args);
            }
        }
    }
    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim
