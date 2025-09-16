#include "mim/pass.h"

#include <memory>

#include <absl/container/fixed_array.h>

#include "mim/driver.h"
#include "mim/phase.h"

#include "mim/util/util.h"

namespace mim {

/*
 * Stage
 */

Stage::Stage(World& world, flags_t annex)
    : world_(world)
    , annex_(annex)
    , name_(world.annex(annex)->sym()) {}

std::unique_ptr<Stage> Stage::recreate() {
    auto ctor = driver().stage(annex());
    auto ptr  = (*ctor)(world());
    ptr->apply(*this);
    return ptr;
}

void Pass::init(PassMan* man) { man_ = man; }

/*
 * PassMan
 */

void PassMan::apply(Passes&& passes) {
    for (auto&& pass : passes)
        if (auto&& man = pass->isa<PassMan>())
            apply(std::move(man->passes_));
        else
            add(std::move(pass));
}

void PassMan::apply(const App* app) {
    auto passes = Passes();
    for (auto arg : app->args())
        if (auto stage = Stage::create(driver().stages(), arg))
            passes.emplace_back(std::unique_ptr<Pass>(static_cast<Pass*>(stage.release())));

    apply(std::move(passes));
}

void PassMan::push_state() {
    if (fixed_point()) {
        states_.emplace_back(passes().size());

        // copy over from prev_state to curr_state
        auto&& prev_state      = states_[states_.size() - 2];
        curr_state().curr_mut  = prev_state.stack.top();
        curr_state().stack     = prev_state.stack;
        curr_state().mut2visit = prev_state.mut2visit;
        curr_state().old_ops.assign(curr_state().curr_mut->ops().begin(), curr_state().curr_mut->ops().end());

        for (size_t i = 0; i != passes().size(); ++i)
            curr_state().data[i] = passes_[i]->copy(prev_state.data[i]);
    }
}

void PassMan::pop_states(size_t undo) {
    while (states_.size() != undo) {
        for (size_t i = 0, e = curr_state().data.size(); i != e; ++i)
            passes_[i]->dealloc(curr_state().data[i]);

        if (undo != 0) // only reset if not final cleanup
            curr_state().curr_mut->set(curr_state().old_ops);

        states_.pop_back();
    }
}

void PassMan::run() {
    world().verify().ILOG("🔥 run");
    for (auto&& pass : passes_)
        ILOG(" 🔹 `{}`", pass->name());
    world().debug_dump();

    auto num = passes().size();
    states_.emplace_back(num);
    for (size_t i = 0; i != num; ++i) {
        auto pass    = passes_[i].get();
        pass->index_ = i;
        pass->init(this);
        curr_state().data[i] = pass->alloc();
    }

    for (auto&& pass : passes_)
        pass->prepare();

    for (auto mut : world().copy_externals()) {
        analyzed(mut);
        if (mut->is_set()) curr_state().stack.push(mut);
    }

    while (!curr_state().stack.empty()) {
        push_state();
        curr_mut_ = pop(curr_state().stack);
        VLOG("⚙️ state {}: `{}`", states_.size() - 1, curr_mut_);

        if (!curr_mut_->is_set()) continue;

        for (auto&& pass : passes_)
            if (pass->inspect()) pass->enter();

        DLOG("curr_mut: {} : {}", curr_mut_, curr_mut_->type());

        auto new_defs = absl::FixedArray<const Def*>(curr_mut_->num_ops());
        for (size_t i = 0, e = curr_mut_->num_ops(); i != e; ++i)
            new_defs[i] = rewrite(curr_mut_->op(i));
        curr_mut_->set(new_defs);

        VLOG("🔍 analyze");
        proxy_    = false;
        auto undo = No_Undo;
        for (auto op : curr_mut_->deps())
            undo = std::min(undo, analyze(op));

        if (undo == No_Undo) {
            assert(!proxy_ && "proxies must not occur anymore after leaving a mut with No_Undo");
            DLOG("=== done ===");
        } else {
            pop_states(undo);
            DLOG("=== undo: {} -> {} ===", undo, curr_state().stack.top());
        }
    }

    world().verify().ILOG("🔒 finished");
    pop_states(0);

    world().debug_dump();

    Phase::run<Cleanup>(world());
}

const Def* PassMan::rewrite(const Def* old_def) {
    if (!old_def->has_dep()) return old_def;

    if (auto mut = old_def->isa_mut()) {
        curr_state().mut2visit.emplace(mut, curr_undo());
        return map(mut, mut);
    }

    if (auto new_def = lookup(old_def)) {
        if (old_def == *new_def)
            return old_def;
        else
            return map(old_def, rewrite(*new_def));
    }

    auto new_type = old_def->type() ? rewrite(old_def->type()) : nullptr;
    auto new_ops  = absl::FixedArray<const Def*>(old_def->num_ops());
    for (size_t i = 0, e = old_def->num_ops(); i != e; ++i)
        new_ops[i] = rewrite(old_def->op(i));
    auto new_def = old_def->rebuild(new_type, new_ops);

    if (auto proxy = new_def->isa<Proxy>()) {
        if (auto&& pass = passes_[proxy->pass()]; pass->inspect()) {
            if (auto rw = pass->rewrite(proxy); rw != proxy) return map(old_def, rewrite(rw));
        }
    } else {
        for (auto&& pass : passes_) {
            if (!pass->inspect()) continue;

            if (auto var = new_def->isa<Var>()) {
                if (auto rw = pass->rewrite(var); rw != var) return map(old_def, rewrite(rw));
            } else {
                if (auto rw = pass->rewrite(new_def); rw != new_def) return map(old_def, rewrite(rw));
            }
        }
    }

    return map(old_def, new_def);
}

undo_t PassMan::analyze(const Def* def) {
    undo_t undo = No_Undo;

    if (!def->has_dep() || analyzed(def)) {
        // do nothing
    } else if (auto mut = def->isa_mut()) {
        if (mut->is_set()) curr_state().stack.push(mut);
    } else if (auto proxy = def->isa<Proxy>()) {
        proxy_ = true;
        undo   = passes_[proxy->pass()]->analyze(proxy);
    } else {
        auto var = def->isa<Var>();
        if (!var)
            for (auto op : def->deps())
                undo = std::min(undo, analyze(op));

        for (auto&& pass : passes_)
            if (pass->inspect()) undo = std::min(undo, var ? pass->analyze(var) : pass->analyze(def));
    }

    return undo;
}

} // namespace mim
