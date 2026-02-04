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
#include "mim/plug/mem/autogen.h"
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

bool is_type_to_consider(const Def* def) {
    auto seq = def->isa<Seq>();
    // use def->world().flags().scalarize_threshold instead?
    return seq && (!Lit::isa(seq->arity()->arity()) || Lit::isa(seq->arity()) > 10) && seq->body()->type()
        && seq->body()->type()->isa<Type>();
}

bool is_tuple_to_consider(const Def* def) { return is_type_to_consider(def->type()); }

// For a new array, we create a new buffer.
const Def* create_buffer(const Def* def, const Def* mem) {
    auto& w = def->world();
    if (is_tuple_to_consider(def)) {
        // Create a new buffer for the seq
        auto ptrType = w.tuple({def->type(), w.lit_nat_0()});
        auto buffer  = w.call<mem::alloc>(ptrType, mem);
        w.DLOG("Bufferize: Allocated {} to buffer {}", def, buffer);
        buffer->dump(2);
        buffer->type()->dump();
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
    for (auto use : usesA)
        defA->world().DLOG("use A: {}", use);
    for (auto use : usesB)
        defA->world().DLOG("use B: {}", use);
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
        for (auto node : nodes)
            node->world().DLOG("  node {}", node);
    }
}

const BufferizationAnalysisInfo& BufferizationAnalysis::get_analysis_info() const { return analysis_info_; }

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

    for (auto op : def->ops())
        analyze(op);
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

    for (auto op : def->ops())
        decide(op);

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

    world().debug_dump();

    auto root = nest.root()->mut()->as_mut<Lam>();

    auto new_root = prep_mut(root)->as<Lam>();

    auto new_filter = visit_def(root->filter());
    auto new_body   = visit_def(root->body());
    new_root->set_filter(new_filter)->set_body(new_body);
    if (root->is_external()) {
        root->internalize();
        new_root->externalize();
    }

    // new_root->dump(100);

    world().DLOG("Bufferize phase finished");
    world().debug_dump();
}

const Def* Bufferize::visit_def(const Def* def) {
    if (auto it = rewritten_.find(def); it != rewritten_.end()) {
        // If we have already rewritten this def, return the rewritten version.
        return it->second;
    }
    if (auto mut = def->isa_mut()) prep_mut(mut);

    auto place = static_cast<Lam*>(sched_.smart(root(), def)->mut());
    place      = visit_def(place)->as_mut<Lam>();

    // do this before new_ops, to handle multi-dimensional arrays.
    if (auto seq = def->type()->isa<Seq>(); seq && is_type_to_consider(seq)) {
        world().DLOG("Bufferizing Seq {}", def);
        if (auto extract = def->isa<Extract>()) {
            if (auto rw = visit_extract(place, extract)) return rw;
        } else if (auto insert = def->isa<Insert>()) {
            if (auto rw = visit_insert(place, insert)) return rw;
        }

        // If the def is a Seq, we check if it can be bufferized.
        if (auto buffer = create_buffer(def, active_mem(place))) {
            // store tuple to buffer.
            auto [mem, ptr] = buffer->projs<2>();
            mem             = world().call<mem::store>(Defs{mem, ptr, def});
            lam2mem_[place].push_back(mem);

            rewritten_[def] = world().tuple(Defs{mem, ptr});
            world().DLOG("Bufferized Seq {} to {}", def, buffer);
            return buffer;
        }
    }

    if (auto app = def->isa<App>(); app && !app->axm()) return rewritten_[def] = visit_app(place, app);

    auto new_ops = DefVec(def->ops(), [&](const Def* op) {
        // Recursively visit each operation in the def.
        return rewritten_[op] = visit_def(op);
    });

    if (auto insert = def->isa<Insert>()) {
        // If the def is an Insert, we handle it specifically.
        if (auto rw = visit_insert(place, insert)) return rw;
    } else if (auto extract = def->isa<Extract>()) {
        // If the def is an Extract, we handle it specifically.
        if (auto rw = visit_extract(place, extract)) return rw;
    }

    if (auto mut = def->isa_mut()) {
        auto new_mut = rewritten_[def]->as_mut()->set(new_ops);
        if (mut->is_external()) {
            new_mut->externalize();
            mut->internalize();
        }
        return rewritten_[new_mut] = new_mut;
    }

    world().DLOG("Rewriting def {}", def);
    for (auto op : new_ops)
        world().DLOG("new op: {}", op);
    return rewritten_[def] = def->rebuild(def->type(), new_ops);
}

const Def* Bufferize::active_mem(Lam* place) {
    if (auto it = lam2mem_.find(place); it != lam2mem_.end()) return world().call<mem::remem>(it->second.back());

    auto mem = world().call<mem::remem>(mem::mem_var(rewritten_[place]->as_mut<Lam>()));
    lam2mem_[place].push_back(mem);
    lam2mem_[rewritten_[place]].push_back(mem);
    return mem;
}

void Bufferize::add_mem(const Lam* place, const Def* mem) { lam2mem_[place].push_back(mem); }

const Def* Bufferize::visit_insert(Lam* place, const Insert* insert) {
    auto tuple = visit_def(insert->tuple());
    auto index = visit_def(insert->index());
    auto value = visit_def(insert->value());

    world().DLOG("Bufferizing Insert {} :{}", insert, insert->type());

    if (is_tuple_to_consider(insert)) {
        auto mem_ptr     = tuple->projs<2>();
        auto& [mem, ptr] = mem_ptr;

        if (analysis_->get_analysis_info().get_choice(insert) == detail::BufferizationChoice::Inplace) {
            // intentionally left blank
            world().DLOG("Bufferizing Insert inplace {}", insert);
        } else if (auto buffer = create_buffer(insert, mem)) {
            // If the tuple is an array, we bufferize it.
            world().DLOG("Created target buffer for Insert {}", insert);

            auto src_ptr = ptr;
            mem_ptr      = buffer->projs<2>();
            mem          = call_copy_array(mem, src_ptr, ptr);
        }

        // store the value in the buffer.
        auto elem_ptr = world().call<mem::lea>(Defs{ptr, index});

        if (is_tuple_to_consider(insert->value())) {
            if (analysis_->get_analysis_info().get_choice(insert->value()) == detail::BufferizationChoice::Inplace) {
                return rewritten_[insert] = world().tuple(Defs{value->proj(0), ptr});
            } else {
                auto [val_mem, val_ptr] = value->projs<2>();
                mem                     = world().call<mem::merge>(mem, val_mem);
                mem                     = call_copy_array(mem, val_ptr, elem_ptr);
                add_mem(place, mem);
                return rewritten_[insert] = world().tuple(Defs{mem, ptr});
            }
        }

        mem = world().call<mem::store>(Defs{mem, elem_ptr, value});

        add_mem(place, mem);
        return rewritten_[insert] = world().tuple(Defs{mem, ptr});
    }

    // If the tuple is not an array, we just let it being rewritten as any other node.
    return nullptr;
}

const Def* Bufferize::visit_extract(Lam* place, const Extract* extract) {
    auto tuple = extract->tuple();

    if (auto it = rewritten_.find(tuple); it != rewritten_.end() && is_tuple_to_consider(tuple)) {
        world().DLOG("Rewriting extract {}", extract);
        world().DLOG("Use rewritten tuple {}", it->second);

        auto index = visit_def(extract->index());

        // If we have already rewritten this tuple, use the rewritten version.
        auto [mem, ptr] = it->second->projs<2>();
        mem             = active_mem(place);

        ptr = world().call<mem::lea>(Defs{ptr, index});

        if (is_tuple_to_consider(extract)) return rewritten_[extract] = world().tuple(Defs{mem, ptr});

        auto [new_mem, value] = world().call<mem::load>(Defs{mem, ptr})->projs<2>();

        add_mem(place, new_mem);
        return rewritten_[extract] = value;
    }
    // If the tuple is not an array, we just let it being rewritten as any other node.
    return nullptr;
}

const Def* Bufferize::visit_app(Lam* place, const App* app) {
    // is arg 0 always mem? :)
    auto new_callee = visit_def(app->callee());
    auto args       = app->args([&](const Def* arg) {
        auto new_def = visit_def(arg);
        world().DLOG("App arg {} rewritten to {}", arg, new_def);
        if (is_tuple_to_consider(arg)) // we only want the ptr part
        {
            world().DLOG("App arg {} is tuple to consider, extracting ptr", arg);
            return new_def->projs<2>()[1];
        }
        return new_def;
    });

    if (Axm::isa<mem::M>(args[0]->type())) {
        auto new_mem_arg = world().call<mem::merge>(active_mem(place));
        args[0]          = new_mem_arg;
        // todo: do we want to clear lam2mem_ and use a potential result mem..? is there a result mem?
    }

    world().DLOG("Bufferize: Rewriting app {} with new args {, }", app, args);
    return world().app(new_callee, args);
}

const Def* Bufferize::visit_type(const Def* type) {
    if (auto it = rewritten_.find(type); it != rewritten_.end()) {
        // If we have already rewritten this def, return the rewritten version.
        return it->second;
    }

    if (type->isa_mut()) {
        // fix me
        rewritten_[type] = type;
    }

    DefVec new_ops = DefVec(type->ops(), [&](const Def* op) {
        // Recursively visit each operation in the def.
        return rewritten_[op] = visit_type(op);
    });

    if (is_type_to_consider(type)) {
        auto ptrType = world().call<mem::Ptr0>(type);
        return ptrType;
    }
    return type->rebuild(type->type(), new_ops);
}

const Def* Bufferize::visit_var(const Def* mem_var, const Def* def, const Def* new_def) {
    if (def->type()->isa<Sigma>()) {
        for (unsigned i = 0; i < def->num_projs(); ++i) {
            auto proj        = def->proj(i);
            auto new_proj    = new_def->proj(i);
            rewritten_[proj] = visit_var(mem_var, proj, new_proj);
        }
    }
    if (is_tuple_to_consider(def)) {
        world().DLOG("replace var proj {} with {}", def, new_def);
        return world().tuple({mem_var, new_def});
    }
    return new_def;
}

Def* Bufferize::prep_mut(Def* mut) {
    auto new_type       = visit_type(mut->type());
    auto new_mut        = mut->stub(new_type);
    rewritten_[mut]     = new_mut;
    rewritten_[new_mut] = new_mut;

    if (auto lam = new_mut->isa_mut<Lam>()) {
        auto new_var               = visit_var(mem::mem_var(lam), mut->var(), new_mut->var());
        rewritten_[mut->var()]     = new_var;
        rewritten_[new_mut->var()] = new_var;
    }
    return new_mut;
}
} // namespace mim::plug::buffrize
