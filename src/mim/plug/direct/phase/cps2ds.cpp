#include "mim/plug/direct/phase/cps2ds.h"

#include <algorithm>
#include <ranges>

#include "mim/def.h"
#include "mim/rewrite.h"

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

    for (auto op : mut->ops())
        init(op);

    return mut;
}

void CPS2DSPhase::start() {
    world().log().set(mim::Log::Level::Debug);
    analyze();

    // Process all visited lams - ignore vars!
    // Vector<Lam*> lamWL;
    for (auto lam : std::ranges::views::filter(visited_, [](const Def* d) { return d->isa_mut<Lam>(); })
                        | std::ranges::views::transform([](const Def* d) { return d->as_mut<Lam>(); })) {
        lamWL_.push_back(lam);
    }

    while (!lamWL_.empty()) {
        auto lam = lamWL_.back();
        lamWL_.pop_back();
        if (auto i = lam2lam_.find(lam); i == lam2lam_.end()) {
            lamStack_.push_back(lam);
            auto filter = rewrite(lam->filter());
            auto body = rewrite(lam->body());
            lamStack_.pop_back();
            lam->set(filter, body);
            lam2lam_[lam] = lam;
        }
    }
    world().log().set(mim::Log::Level::Info);
}

const Def* CPS2DSPhase::rewrite(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if(visited_.find(def) != visited_.end())
        return def;
    if (auto lam = def->isa_mut<Lam>()) {
        lamWL_.push_back(lam);
        visited_.insert(lam);
        return lam;
    }

    if (auto app = def->isa<App>()) {
        auto callee   = rewrite(app->callee());
        auto call_arg = rewrite(app->arg());
        if (auto cps2ds = Axm::isa<direct::cps2ds_dep>(callee)) {
            cps2ds->dump(2);
            cps2ds->arg()->dump(2);

            auto lam = curr_lam();

            auto cn_dom = cps2ds->arg()->as<Lam>()->ret_dom();
            auto cont   = make_continuation(lam->body(), cn_dom, app, lam->sym());
            // lam2lam_[lam] = cont;
            cont->dump(2);

            lamWL_.push_back(cont);
            lam->app(lam->filter(), cps2ds->arg(), world().tuple({call_arg, cont}));
            lam->dump(2);
            cont->dump(2);
            return rewritten_[def] = cont->var();
        }
    }

    DefVec new_ops{def->ops(), [this](const Def* d) { return rewrite(d); }};
    auto new_def    = def->rebuild(def->type(), new_ops);
    rewritten_[def] = new_def;
    return new_def;
}

Lam* CPS2DSPhase::make_continuation(const Def* body, const Def* cn_type, const Def* arg, Sym prefix) {
    world().DLOG("make_continuation for body {} : {} with cn_type {}", body, body->type(), cn_type);
    auto name = world().append_suffix(prefix, "_cps2ds_cont");
    auto lam  = world().mut_con(cn_type)->set(name)->set_filter(false);

    rewritten_[arg] = lam->var();
    auto body_rw = rewrite(body);
    lam->set_body(body_rw);

    return lam;
}

} // namespace mim::plug::direct
