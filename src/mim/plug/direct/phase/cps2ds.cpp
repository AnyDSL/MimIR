#include "mim/plug/direct/phase/cps2ds.h"

#include <algorithm>
#include <ranges>

#include "mim/def.h"
#include "mim/rewrite.h"
#include "mim/schedule.h"
#include "mim/world.h"

#include "mim/util/print.h"
#include "mim/util/vector.h"

#include "mim/plug/direct/autogen.h"

namespace mim::plug::direct {

bool CPS2DSPhase::analyze() {
    // Traverse annexes and externals to initialize any mutables we need.
    for (auto def : world().annexes())
        init(def);
    for (auto def : world().externals().muts())
        init(def);

    return false; // no fixed-point required here
}

const Def* CPS2DSPhase::init(const Def* def) {
    if (auto mut = def->isa_mut()) return init(mut);

    for (auto op : def->ops())
        init(op);
    return def;
}

const Def* CPS2DSPhase::init(Def* mut) {
    if (visited_.contains(mut)) return mut;

    // Mark the mutable (or its var) as visited so the phase can track encountered defs.
    if (auto var = mut->has_var()) visited_.emplace(var);
    visited_.emplace(mut);

    if (!mut->is_set()) return mut;

    for (auto op : mut->ops())
        init(op);

    return mut;
}

void CPS2DSPhase::start() {
    // world().log().set(mim::Log::Level::Debug);
    world().debug_dump();
    // analyze();

    // Process all visited lams - ignore vars!
    // auto rg = std::ranges::views::filter(visited_, [](const Def* d) { return d->isa_mut<Lam>(); })
    //                     | std::ranges::views::transform([](const Def* d) { return d->as_mut<Lam>(); });
    // lamWL_.insert(lamWL_.end(), rg.begin(), rg.end());

    visited_.clear();
    scheduler_.clear();
    nests_.clear();
    lam2lam_.clear();
    rewritten_.clear();

    world().for_each(true, [this](Def* mut) {
        if (auto lam = mut->isa_mut<Lam>(); lam && !lam->codom()->isa<Type>()) nests_.insert({lam, Nest(lam)});
    });

    for (auto external : world().externals().muts())
        if (auto lam = external->isa_mut<Lam>()) {
            current_external_ = lam;
            rewrite_lam(lam);
        }

    world().debug_dump();

    // while (!lamWL_.empty()) {
    //     auto lam = lamWL_.back();
    //     lamWL_.pop_back();

    //     if (!lam->is_set()) continue;
    //     if (auto i = lam2lam_.find(lam); i == lam2lam_.end()) {
    //         lamStack_.push_back(lam);
    //         world().DLOG("Rewriting lam: {}", lam);

    //         auto filter = rewrite(lam->filter());
    //         auto body = rewrite(lam->body());

    //         lamStack_.pop_back();
    //         lam->set(filter, body);
    //         lam2lam_[lam] = lam;
    //     }
    // }
}

const Def* CPS2DSPhase::rewrite_lam(Lam* lam) {
    if (auto i = rewritten_.find(lam); i != rewritten_.end()) return i->second;
    if (visited_.contains(lam)) return lam;
    if (lam->isa_imm() || !lam->is_set() || lam->codom()->isa<Type>()) {
        world().DLOG("skipped {}", lam);
        return lam;
    }

    visited_.insert(lam);

    lam2lam_[lam] = lam;
    // curr_lam_stack().push_back(lam);
    world().DLOG("Rewriting lam: {}", lam->unique_name());

    auto filter = rewrite(lam->filter());

    if (auto body = lam->body()->isa<App>(); !body) {
        world().DLOG("  non-app body {}, skipped", lam->body());
        // curr_lam_stack().pop_back();
        auto new_body = rewrite(lam->body());
        lam->unset()->set(filter, new_body);
        return rewritten_[lam] = lam;
    }

    auto body       = lam->body()->as<App>();
    auto new_arg    = rewrite(body->arg());

    auto new_lam = result_lam(lam);
    world().DLOG("Result of rewrite {} set for {}", lam->unique_name(), new_lam->unique_name());
    auto new_callee = rewrite(body->callee());

    if (world().log().level() >= mim::Log::Level::Debug) body->dump(1);

    new_lam->unset()->app(filter, new_callee, new_arg);

    return rewritten_[lam] = lam;
}

const Def* CPS2DSPhase::rewrite(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (visited_.find(def) != visited_.end()) return def;
    if (auto lam = def->isa_mut<Lam>()) {
        // lamWL_.push_back(lam);
        // visited_.insert(lam);
        return rewrite_lam(lam);
    }

    if (auto app = def->isa<App>()) {
        // auto uncurried_callee = app->uncurry_callee();
        // uncurried_callee->dump(2);
        auto [axm, curry, trip] = Axm::get(app);
        world().DLOG("axiom: {} curry: {} trip: {}", axm, (uint16_t)curry, (uint16_t)trip);
        if (auto cps2ds = Axm::isa<direct::cps2ds_dep>(app->callee())) {
            // push();
            // curr_lam_stack().push_back(cps2ds->arg()->as_mut<Lam>());
            auto cps_lam = rewrite(cps2ds->arg())->as<Lam>();
            // pop();

            auto call_arg = rewrite(app->arg());

            if (world().log().level() >= mim::Log::Level::Debug) {
                cps2ds->dump(2);
                cps2ds->arg()->dump(2);
            }

            // auto lam = curr_lam();
            auto node = scheduler(app).smart(current_external_, app);
            auto lam  = result_lam(node->mut()->as_mut<Lam>());
            world().DLOG("current lam: {} : {}", lam->unique_name(), lam->type());

            auto cn_dom = cps_lam->ret_dom();
            auto cont   = make_continuation(cn_dom, app, cps_lam->sym());
            world().DLOG("continuation created: {} : {}", cont, cont->type());
            if (world().log().level() >= mim::Log::Level::Debug) cont->dump(2);
            {
                auto filter = rewritten_[lam->filter()] = rewrite(lam->filter());
                auto body                               = world().app(cps_lam, world().tuple({call_arg, cont}));
                rewritten_[lam]                         = lam->unset()->set(filter, body);
                lam2lam_[lam]                           = cont;
            }
            world().DLOG("point the lam to the cont: {} = {}", lam->unique_name(), lam->body());
            if (world().log().level() >= mim::Log::Level::Debug) {
                lam->dump(2);
                cont->dump(2);
            }

            // curr_lam_stack().pop_front();
            // curr_lam_stack().push_front(cont);
            // rewrite_lam(cont);
            // auto new_cont_body = rewrite(cont->body());
            // auto filter        = cont->filter();
            // cont->unset()->set(filter, new_cont_body);
            // world().DLOG("rewritten continuation body: {} = {}", cont, cont->body());
            // cont->dump(2);

            return cont->var(); // rewritten_[def] = cont->var(); done in make_continuation
        }
    }

    DefVec new_ops{def->ops(), [this](const Def* d) { return rewrite(d); }};
    auto new_def    = def->rebuild(def->type(), new_ops);
    rewritten_[def] = new_def;
    return new_def;
}

Lam* CPS2DSPhase::make_continuation(const Def* cn_type, const Def* arg, Sym prefix) {
    world().DLOG("make_continuation {} : {} ({})", prefix, cn_type, arg);
    if (world().log().level() >= mim::Log::Level::Debug) arg->dump(2);
    auto name = world().append_suffix(prefix, "_cps2ds_cont");
    auto cont = world().mut_con(cn_type)->set(name)->set_filter(false);

    rewritten_[arg] = cont->var();
    // auto body_rw = rewrite(body);
    // Rewriter rw{world()};
    // rw.map(arg, cont->var());
    // auto body_rw = rw.rewrite(body);
    // cont->set_body(body_rw);

    return cont;
}

Lam* CPS2DSPhase::result_lam(Lam* lam) {
    if (auto i = lam2lam_.find(lam); i != lam2lam_.end())
        if (i->second != lam) return result_lam(i->second);
    return lam;
}

Scheduler& CPS2DSPhase::scheduler(const Def* def) {
    auto get_or_make = [&](const Def* lam, const Nest& nest) -> Scheduler& {
        if (auto sched = scheduler_.find(lam); sched != scheduler_.end()) {
            world().DLOG("found existing scheduler for {}", lam);
            return sched->second;
        } else {
            world().DLOG("creating new scheduler for {}", lam);
            auto [it, inserted] = scheduler_.insert({lam, Scheduler(nest)});
            return it->second;
        }
    };
    for (auto& [lam, nest] : nests_) {
        world().DLOG("looking for scheduler in {} for {}", lam, def);
        if (nest.contains(def)) return get_or_make(lam, nest);
    }
    world().DLOG("no scheduler found for {}, using current external {}", def, current_external_);
    return get_or_make(current_external_, curr_external_nest());
    // error("Should have found nest for def {}", def);
}

const Nest& CPS2DSPhase::curr_external_nest() const {
    auto i = nests_.find(current_external_);
    assert(i != nests_.end());
    return i->second;
}

} // namespace mim::plug::direct
