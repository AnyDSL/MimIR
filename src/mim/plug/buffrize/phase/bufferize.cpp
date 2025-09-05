#include "mim/plug/buffrize/phase/bufferize.h"

#include <functional>
#include <memory>
#include <stdexcept>

#include "mim/def.h"
#include "mim/schedule.h"
#include "mim/world.h"

#include "mim/plug/affine/affine.h"
#include "mim/plug/buffrize/autogen.h"
#include "mim/plug/buffrize/buffrize.h"
#include "mim/plug/core/core.h"
#include "mim/plug/direct/direct.h"
#include "mim/plug/mem/mem.h"

#include "absl/container/flat_hash_map.h"

namespace mim::plug::buffrize {
namespace {

const Def* call_copy_array(const Def* mem, const Def* src, const Def* dst) {
    auto& w = src->world();
    if (src == dst) return mem; // No need to copy if they are the same
    w.DLOG("Copying array from {} to {}", src, dst);

    auto copy_annex = w.call<buffrize::copy_array>(Defs{src, dst});

    auto new_mem = w.call<direct::cps2ds>(Defs{w.annex<mem::M>(), w.annex<mem::M>()}, copy_annex, mem);
    return new_mem;
}

bool is_tuple_to_consider(const Def* def) {
    auto seq = def->type()->isa<Seq>();
    return seq && (!seq->shape()->isa_lit_arity() || seq->isa_lit_arity() > 10) && seq->body()->type()
        && seq->body()->type()->isa<Type>();
}

// For a new array, we create a new buffer.
const Def* create_buffer(const Def* def, const Def* mem) {
    auto& w = def->world();
    if (is_tuple_to_consider(def)) {
        // Create a new buffer for the seq
        auto ptrType = w.tuple({def->type(), w.lit_nat_0()});
        auto buffer  = w.call<mem::alloc>(ptrType, mem);
        w.DLOG("Bufferize: Allocated {} to buffer {}", def, buffer);
        return buffer;
    }
    return nullptr;
}
} // namespace

namespace detail {
const Def* AliasSet::find(const Def* def) {
    auto it = parent_.find(def);
    if (it == parent_.end() || it->second == def) return def;
    assign_new_parent(it->second, find(it->second)); // Path compression.
    return parent_[def];
}

const Def* AliasSet::find(const Def* def) const {
    auto it = parent_.find(def);
    if (it == parent_.end() || it->second == def) return def;
    return find(it->second);
}

void AliasSet::unite(const Def* def1, const Def* def2) {
    const Def* root1 = find(def1);
    const Def* root2 = find(def2);
    root1->world().DLOG("unite {} -- {} - {} -- {}", def1, def2, root1, root2);
    if (root1 != root2) assign_new_parent(root1, root2); // Make one root point to the other.
}

DefSet& AliasSet::get_maybe_new_set(const Def* def) {
    if (auto it = sets_.find(def); it != sets_.end()) return it->second;

    sets_[def] = {def};
    return sets_[def];
}

void AliasSet::assign_new_parent(const Def* def1, const Def* def2) {
    parent_[def1] = def2;
    if (def1 == def2) return;

    auto def1_set  = get_maybe_new_set(def1);
    auto& def2_set = get_maybe_new_set(def2);
    def2_set.insert(def1_set.begin(), def1_set.end());
    sets_.erase(def1);
}

InterferenceSchedule::InterferenceSchedule(Scheduler& sched)
    : sched_(sched) {
    Lam* root = sched_.root()->as_mut<Lam>();
    schedule(root, root->body());
}

void InterferenceSchedule::schedule(Lam* mut, const Def* def) {
    if (!seen_.insert(def).second) return;

    auto loc = sched_.smart(mut, def);

    for (const Def* op : def->ops()) {
        def_use_map_[op].insert(def);
        schedule(mut, op);
    }

    // if (is_tuple_to_consider(def) || std::any_of(def->ops().begin(), def->ops().end()))
    ordered_schedule_[loc->mut()].push_back(def);
}

InterferenceSchedule::NodeOrder InterferenceSchedule::get_order(const Def* a, const Def* b) {
    a->world().DLOG("Getting order for {} -- {}", a, b);
    if (a == b) {
        a->world().DLOG("order: SAME");
        return SAME;
    }
    auto locA = sched_.smart(sched_.root(), a);
    auto locB = sched_.smart(sched_.root(), b);

    if (locA != locB) {
        a->world().DLOG("order: INDEPENDENT");
        return INDEPENDENT;
    }

    auto& lam = ordered_schedule_[locA->mut()];

    auto itA = std::find(lam.cbegin(), lam.cend(), a);
    auto itB = std::find(lam.cbegin(), lam.cend(), b);
    if (itA == lam.cend() || itB == lam.cend()) {
        a->world().DLOG("order: INDEPENDENT");
        return INDEPENDENT;
    }
    if (itA < itB) {
        a->world().DLOG("order: BEFORE");
        return BEFORE;
    } else if (itA > itB) {
        a->world().DLOG("order: AFTER");
        return AFTER;
    }
    throw std::runtime_error{"get_order should always find an order for two values of the same Lam."};
}

InterferenceSchedule::Interference InterferenceSchedule::get_interference(const Def* defA, const Def* defB) {
    defA->world().DLOG("Getting Interference for {} -- {}", defA, defB);
    if (defA == defB) return NONE;

    // auto& usesA = sched_.uses(defA);
    // auto& usesB = sched_.uses(defB);
    auto usesA = def_use_map_[defA];
    auto usesB = def_use_map_[defB];
    for (auto use : usesA) defA->world().DLOG("use A: {}", use);
    for (auto use : usesB) defA->world().DLOG("use B: {}", use);
    if (usesA.empty() || usesB.empty()) return NONE;

    auto defOrder = get_order(defA, defB);

    auto comparison = [this, defOrder, defA, defB]() -> std::function<Interference(const Def*, const Def*)> {
        switch (defOrder) {
            case BEFORE:
                return [this, defA, defB](const Def* useA, const Def* useB) {
                    switch (get_order(useA, defB)) {
                        case BEFORE: break;
                        case AFTER: return YES;
                        case INDEPENDENT:
                        case SAME: break;
                    }
                    return NONE;
                };
            case AFTER:
                return [this, defA, defB](const Def* useA, const Def* useB) {
                    switch (get_order(defA, useB)) {
                        case BEFORE: return YES;
                        case AFTER:
                        case INDEPENDENT:
                        case SAME: break;
                    }
                    return NONE;
                };
            case INDEPENDENT:
            case SAME: return [](const Def*, const Def*) { return NONE; };
        }
    }();

    for (auto useA : usesA) {
        for (auto useB : usesB)
            if (comparison(useA, useB) == YES) {
                useA->world().DLOG("{}--{} interferes with {}--{}", defA, useA, defB, useB);
                return YES;
            }
    }
    defA->world().DLOG("{} doesnt interfere with {}", defA, defB);
    return NONE;
}

void InterferenceSchedule::dump() const {
    for (auto [lam, nodes] : ordered_schedule_) {
        lam->world().DLOG("lam {}", lam->unique_name());
        for (auto node : nodes) node->world().DLOG("  node {}", node);
    }
}

const BufferizationAnalysisInfo& BufferizationAnalysis::get_analysis_infO() const { return analysis_info_; }

void BufferizationAnalysis::analyze(const Def* def) {
    if (!seen_.insert(def).second) return;

    switch (def->node()) {
        case Node::Insert: analysis_info_.add_alias(def, def->as<Insert>()->tuple()); break;
        case Node::Extract:
            // don't care about uses here..
        case Node::Lit:
            // don't need to do anything here, is added as alias by users
            break;
        // case Node::Axm:
        case Node::Var:
        // case Node::Global:
        // case Node::Proxy:
        // case Node::Hole:
        case Node::Type:
        case Node::Univ:
        // case Node::UMax:
        // case Node::UInc:
        // case Node::Pi:
        case Node::Lam:
        case Node::App:

        // case Node::Sigma:
        case Node::Tuple:
        case Node::Arr:
        case Node::Pack:
            // case Node::Join:
            // case Node::Inj:
            // case Node::Match:
            // case Node::Top:
            // case Node::Meet:
            // case Node::Merge:
            // case Node::Split:
            // case Node::Bot:
            // case Node::Uniq:
            // case Node::Nat:
            // case Node::Idx:
            break;
        default: break;
    }

    for (auto op : def->ops()) analyze(op);
}

BufferizationAnalysis::BufferizationAnalysis(const Nest& nest, Scheduler& sched)
    : nest_(nest)
    , interference_(sched) {
    interference_.dump();
    analyze();
}

void BufferizationAnalysis::analyze() {
    if (!nest_.root()->mut()->isa_mut<Lam>()) {
        world().DLOG("Bufferization analysis: root is not a mutable Lam, skipping analysis");
        return;
    }
    auto root = static_cast<Lam*>(nest_.root()->mut());
    world().DLOG("Starting bufferization analysis for nest: {}", nest_.root()->name());

    analyze(root->filter());
    analyze(root->body());

    seen_.clear();

    decide(root->filter());
    decide(root->body());
}

void BufferizationAnalysis::decide(const Def* def) {
    if (!seen_.insert(def).second) return;

    for (auto op : def->ops()) decide(op);

    if (is_tuple_to_consider(def)) {
        def->world().DLOG("deciding bufferization for {}", def);
        if (def->isa<Seq>()) {
            analysis_info_.must_allocate(def);
            return;
        }
        def->world().DLOG("alias set size {}", analysis_info_.get_alias_set(def).size());

        for (auto* alias : analysis_info_.get_alias_set(def)) {
            if (interference_.get_order(def, alias) == InterferenceSchedule::BEFORE) {
                if (interference_.get_interference(def, alias) == InterferenceSchedule::YES)
                    analysis_info_.must_copy(alias);
            }
        }
    }
}
} // namespace detail

void Bufferize::visit(const Nest& nest) {
    if (auto it = rewritten_.find(nest.root()->mut()); it != rewritten_.end()) {
        // If we have already rewritten the root, we can skip the bufferization.
        world().DLOG("Bufferize: Skipping already rewritten root {}", it->second);
        return;
    }

    world().DLOG("Bufferize phase started");
    world().DLOG("Bufferize phase for nest: {}", nest.root()->name());
    if (!nest.root()->mut()->isa_mut<Lam>()) {
        world().DLOG("Bufferize phase: root is not a mutable Lam, skipping bufferization");
        return;
    }
    nest.root()->mut()->dot(nest.root()->name() + "_bufferize.dot");

    sched_ = Scheduler{nest};
    analysis_.reset(new detail::BufferizationAnalysis(nest, sched_));

    auto root = static_cast<Lam*>(nest.root()->mut());

    auto new_filter = visit_def(root->filter());
    auto new_body   = visit_def(root->body());
    if (auto app = new_body->isa<App>()) {
        // is arg 0 alwasy mem? :)
        auto new_mem_arg = world().call<mem::merge>(lam2mem_[root]);
        auto new_args    = app->args();
        DefVec new_args_vec(new_args.size(), [&](size_t i) {
            if (i == 0) return new_mem_arg;
            return new_args[i];
        });
        world().DLOG("Bufferize: Rewriting app {} with new args {}", app, new_args_vec);
        // todo: is callee necessarily a mut we want to reuse?
        root->unset()->app(new_filter, app->callee(), new_args_vec);
    } else {
        root->unset()->set({new_filter, new_body});
    }
    world().DLOG("Bufferize phase finished");
    world().debug_dump();
}

const Def* Bufferize::visit_def(const Def* def) {
    if (auto it = rewritten_.find(def); it != rewritten_.end()) {
        // If we have already rewritten this def, return the rewritten version.
        return it->second;
    }
    if (def->isa_mut()) {
        // fix me
        rewritten_[def] = def;
    }

    auto new_ops = DefVec(def->ops(), [&](const Def* op) {
        // Recursively visit each operation in the def.
        return rewritten_[op] = visit_def(op);
    });

    auto place = static_cast<Lam*>(sched_.smart(root(), def)->mut());
    if (auto tuple = def->isa<Seq>()) {
        world().DLOG("Bufferizing Seq {}", tuple);
        // If the def is a Seq, we check if it can be bufferized.
        if (auto buffer = create_buffer(tuple, active_mem(place))) {
            // store tuple to buffer.
            auto [mem, ptr] = buffer->projs<2>();
            mem             = world().call<mem::store>(Defs{mem, ptr, tuple});
            lam2mem_[place].push_back(mem);

            rewritten_[def] = world().tuple(Defs{mem, ptr});
            world().DLOG("Bufferized Seq {} to {}", tuple, buffer);
            return buffer;
        }
    } else if (auto insert = def->isa<Insert>()) {
        // If the def is an Insert, we handle it specifically.
        return visit_insert(place, insert);
    } else if (auto extract = def->isa<Extract>()) {
        // If the def is an Extract, we handle it specifically.
        return visit_extract(place, extract);
    }
    if (auto mut = def->isa_mut()) {
        mut->unset()->set(new_ops);
        return mut;
    }
    return rewritten_[def] = def->rebuild(def->type(), new_ops);
}

const Def* Bufferize::active_mem(Lam* place) {
    if (auto it = lam2mem_.find(place); it != lam2mem_.end()) return world().call<mem::remem>(it->second.back());

    auto mem = world().call<mem::remem>(mem::mem_var(place));
    lam2mem_[place].push_back(mem);
    return mem;
}

void Bufferize::add_mem(const Lam* place, const Def* mem) { lam2mem_[place].push_back(mem); }

const Def* Bufferize::visit_insert(Lam* place, const Insert* insert) {
    auto tuple = rewritten_[insert->tuple()];
    auto index = rewritten_[insert->index()];
    auto value = rewritten_[insert->value()];

    auto mem_ptr     = tuple->projs<2>();
    auto& [mem, ptr] = mem_ptr;

    world().DLOG("Bufferizing Insert {}", insert);

    if (is_tuple_to_consider(insert)) {
        if (analysis_->get_analysis_infO().get_choice(insert) == detail::BufferizationChoice::Inplace) {
        } else if (auto buffer = create_buffer(insert, mem)) {
            // If the tuple is an array, we bufferize it.
            world().DLOG("Created target buffer for Insert {}", insert);

            auto src_ptr = ptr;
            mem_ptr      = buffer->projs<2>();
            mem          = call_copy_array(mem, src_ptr, ptr);
        }

        // store the value in the buffer.
        auto elem_ptr = world().call<mem::lea>(Defs{ptr, index});
        mem           = world().call<mem::store>(Defs{mem, elem_ptr, value});

        add_mem(place, mem);
        return rewritten_[insert] = world().tuple(Defs{mem, ptr});
    }

    // If the tuple is not an array, we just return the original insert.
    return insert;
}

const Def* Bufferize::visit_extract(Lam* place, const Extract* extract) {
    auto tuple = extract->tuple();
    auto index = rewritten_[extract->index()];

    if (auto it = rewritten_.find(tuple); it != rewritten_.end() && is_tuple_to_consider(tuple)) {
        world().DLOG("Rewriting extract {}", extract);
        std::cerr.flush();
        std::cout.flush();
        // If we have already rewritten this tuple, use the rewritten version.
        auto [mem, ptr] = it->second->projs<2>();
        mem             = active_mem(place);

        ptr                   = world().call<mem::lea>(Defs{ptr, index});
        auto [new_mem, value] = world().call<mem::load>(Defs{mem, ptr})->projs<2>();
        // rewritten_[tuple]     = world().tuple(Defs{mem, ptr});

        add_mem(place, new_mem);
        return rewritten_[extract] = value;
    }
    // If the tuple is not an array, we just return the original extract.
    return extract;
}

// const Def* Bufferize::remem() { return world().call<mem::remem>(mem_); }

} // namespace mim::plug::buffrize
