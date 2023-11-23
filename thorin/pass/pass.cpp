#include "thorin/pass/pass.h"

#include "thorin/phase/phase.h"
#include "thorin/util/util.h"

#include "absl/container/fixed_array.h"

namespace thorin {

Pass::Pass(PassMan& man, std::string_view name)
    : man_(man)
    , name_(name)
    , index_(man.passes().size()) {}

void PassMan::push_state() {
    if (fixed_point()) {
        states_.emplace_back(passes().size());

        // copy over from prev_state to curr_state
        auto&& prev_state      = states_[states_.size() - 2];
        curr_state().curr_mut  = prev_state.stack.top();
        curr_state().stack     = prev_state.stack;
        curr_state().mut2visit = prev_state.mut2visit;
        curr_state().old_ops.assign(curr_state().curr_mut->ops().begin(), curr_state().curr_mut->ops().end());

        for (size_t i = 0; i != passes().size(); ++i) curr_state().data[i] = passes_[i]->copy(prev_state.data[i]);
    }
}

void PassMan::pop_states(size_t undo) {
    while (states_.size() != undo) {
        for (size_t i = 0, e = curr_state().data.size(); i != e; ++i) passes_[i]->dealloc(curr_state().data[i]);

        if (undo != 0) // only reset if not final cleanup
            curr_state().curr_mut->reset(curr_state().old_ops);

        states_.pop_back();
    }
}

void PassMan::run() {
    world().ILOG("run");

    auto num = passes().size();
    states_.emplace_back(num);
    for (size_t i = 0; i != num; ++i) curr_state().data[i] = passes_[i]->alloc();

    for (auto&& pass : passes_) world().ILOG(" + {}", pass->name());
    world().debug_dump();

    for (auto&& pass : passes_) pass->prepare();

    auto externals = world().externals();
    for (const auto& [_, mut] : externals) {
        analyzed(mut);
        if (mut->is_set()) curr_state().stack.push(mut);
    }

    while (!curr_state().stack.empty()) {
        push_state();
        curr_mut_ = pop(curr_state().stack);
        world().VLOG("=== state {}: {} ===", states_.size() - 1, curr_mut_);

        if (!curr_mut_->is_set()) continue;

        for (auto&& pass : passes_)
            if (pass->inspect()) pass->enter();

        curr_mut_->world().DLOG("curr_mut: {} : {}", curr_mut_, curr_mut_->type());
        for (size_t i = 0, e = curr_mut_->num_ops(); i != e; ++i) curr_mut_->reset(i, rewrite(curr_mut_->op(i)));

        world().VLOG("=== analyze ===");
        proxy_    = false;
        auto undo = No_Undo;
        for (auto op : curr_mut_->extended_ops()) undo = std::min(undo, analyze(op));

        if (undo == No_Undo) {
            assert(!proxy_ && "proxies must not occur anymore after leaving a mut with No_Undo");
            world().DLOG("=== done ===");
        } else {
            pop_states(undo);
            world().DLOG("=== undo: {} -> {} ===", undo, curr_state().stack.top());
        }
    }

    world().ILOG("finished");
    pop_states(0);

    world().debug_dump();
    Phase::run<Cleanup>(world());
}

Ref PassMan::rewrite(Ref old_def) {
    if (!old_def->dep()) return old_def;

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
    auto new_ops  = Vector<const Def*>(old_def->num_ops());
    for (size_t i = 0, e = old_def->num_ops(); i != e; ++i) new_ops[i] = rewrite(old_def->op(i));
    auto new_def = old_def->rebuild(world(), new_type, new_ops);

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

undo_t PassMan::analyze(Ref def) {
    undo_t undo = No_Undo;

    if (!def->dep() || analyzed(def)) {
        // do nothing
    } else if (auto mut = def->isa_mut()) {
        if (mut->is_set()) curr_state().stack.push(mut);
    } else if (auto proxy = def->isa<Proxy>()) {
        proxy_ = true;
        undo   = passes_[proxy->pass()]->analyze(proxy);
    } else {
        auto var = def->isa<Var>();
        if (!var)
            for (auto op : def->extended_ops()) undo = std::min(undo, analyze(op));

        for (auto&& pass : passes_)
            if (pass->inspect()) undo = std::min(undo, var ? pass->analyze(var) : pass->analyze(def));
    }

    return undo;
}

} // namespace thorin
