#include "mim/plug/buffrize/phase/bufferize.h"

#include "mim/def.h"

#include "mim/plug/affine/affine.h"
#include "mim/plug/buffrize/autogen.h"
#include "mim/plug/buffrize/buffrize.h"
#include "mim/plug/core/core.h"
#include "mim/plug/direct/direct.h"
#include "mim/plug/mem/mem.h"

#include "absl/container/flat_hash_map.h"

namespace mim::plug::buffrize {
namespace {

// Union-Find data structure to keep track of equivalence classes of defs.
class UnionFind {
public:
    UnionFind() = default;

    // Find the root of the element with path compression.
    const Def* find(const Def* def) {
        if (parent_.find(def) == parent_.end() || parent_[def] == def) return def;
        parent_[def] = find(parent_[def]); // Path compression.
        return parent_[def];
    }

    // Union two elements.
    void unite(const Def* def1, const Def* def2) {
        const Def* root1 = find(def1);
        const Def* root2 = find(def2);
        if (root1 != root2) parent_[root1] = root2; // Make one root point to the other.
    }

private:
    absl::flat_hash_map<const Def*, const Def*> parent_;
};

class BufferizationAnalysisInfo {
public:
    // Ptr alias union find
    UnionFind ptr_union_find;
};

const Def* build_copy_array(const Def* mem, const Def* src, const Def* dst, const Def* src_arity) {
    auto& w = src->world();
    if (src == dst) return mem; // No need to copy if they are the same
    w.DLOG("Copying array from {} to {}", src, dst);

    auto exitT               = w.cn(w.annex<mem::M>());
    auto wrapper             = w.mut_con({w.annex<mem::M>(), exitT});
    auto [wrapper_mem, exit] = wrapper->vars<2>();

    auto body = w.mut_con(Defs{w.type_idx(0_u64), w.annex<mem::M>(), exitT});
    {
        auto [index, mem, yield] = body->vars<3>();
        auto converted_index     = w.call(core::conv::u, src_arity, index);
        auto src_lea             = w.call<mem::lea>(Defs{src, converted_index});
        auto dst_lea             = w.call<mem::lea>(Defs{dst, converted_index});
        auto [load_mem, value]   = w.call<mem::load>(Defs{mem, src_lea})->projs<2>();
        body->app(false, yield, w.call<mem::store>(Defs{load_mem, dst_lea, value}));
    }

    auto arity_conv = w.call<core::idx>(w.lit_nat_0(), w.lit_nat(static_cast<nat_t>(core::Mode::none)), src_arity);
    auto loop = w.call<affine::For>(Defs{w.lit_idx(0_u64), arity_conv, w.lit_idx(1_u64), wrapper_mem, body, exit});

    wrapper->set(false, loop);

    auto new_mem = w.call<direct::cps2ds>(Defs{w.annex<mem::M>(), w.annex<mem::M>()}, wrapper, mem);

    w.DLOG("Created affine.For loop for copying array contents: {}", loop);
    // new_mem->dump(100);
    return new_mem;
}

const Def* call_copy_array(const Def* mem, const Def* src, const Def* dst, const Def* src_arity) {
    auto& w = src->world();
    if (src == dst) return mem; // No need to copy if they are the same
    w.DLOG("Copying array from {} to {}", src, dst);

    auto copy_annex = w.call<buffrize::copy_array>(Defs{src, dst});

    auto new_mem = w.call<direct::cps2ds>(Defs{w.annex<mem::M>(), w.annex<mem::M>()}, copy_annex, mem);
    // auto new_mem = w.call<buffrize::copy_array>(Defs{mem, src, dst});
    return new_mem;
}

bool is_tuple_to_consider(const Def* def) {
    auto seq = def->type()->isa<Seq>();
    // use def->world().flags().scalarize_threshold instead?
    return seq && (!Lit::isa(seq->arity()->arity()) || Lit::isa(seq->arity()) > 10) && seq->body()->type()
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

    sched_ = Scheduler{nest};
    nest.root()->mut()->dot(nest.root()->name() + "_bufferize.dot");
    // nest.dot(nest.root()->name() + "_bufferize.dot");

    auto root = static_cast<Lam*>(nest.root()->mut());
    // if(auto lam = root->isa<Lam>())
    //     mem_ = mem::mem_var(lam);
    // else
    //     mem_ = world().annex<mem::m>();

    // // Convert all arrays to buffers.
    // auto new_ops = DefVec{root->ops(), [&](const Def* def) {
    //                      // Visit each def and bufferize it if necessary.
    //                      return visit_def(def);
    //                  }};
    // root->unset()->set(new_ops);
    auto new_filter = visit_def(root->filter());
    auto new_body   = visit_def(root->body());
    if (auto app = new_body->isa<App>()) {
        auto new_mem_arg = world().call<mem::merge>(lam2mem_[root]);
        auto new_args    = app->args();
        DefVec new_args_vec(new_args.size(), [&](size_t i) {
            if (i == 0) return new_mem_arg;
            return new_args[i];
        });
        world().DLOG("Bufferize: Rewriting app {} with new args {}", app, new_args_vec);
        root->unset()->app(new_filter, app->callee(), new_args_vec);
    } else {
        root->unset()->set({new_filter, new_body});
    }
    // for (const auto& op : root->ops()) {
    //     // Visit each operation in the root and bufferize it.
    //     auto rewritten = visit_def(op);
    //     if (rewritten != op) world().DLOG("Rewritten {} to {}", op, rewritten);
    // //     root->set(op->index(), rewritten);
    // }
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
    auto tuple = insert->tuple();
    auto index = insert->index();
    auto value = insert->value();

    auto [tuple_mem, tuple_ptr] = rewritten_[tuple]->projs<2>();

    world().DLOG("Bufferizing Insert {}", insert);
    // If the tuple is an array, we bufferize it.
    if (auto buffer = create_buffer(insert, tuple_mem)) {
        world().DLOG("Created target buffer for Insert {}", insert);

        auto [mem, ptr] = buffer->projs<2>();
        mem             = call_copy_array(mem, tuple_ptr, ptr, insert->type()->as<Seq>()->arity());
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
    auto index = extract->index();

    if (auto it = rewritten_.find(tuple); it != rewritten_.end() && is_tuple_to_consider(tuple)) {
        // If we have already rewritten this tuple, use the rewritten version.
        auto [mem, ptr] = it->second->projs<2>();
        mem             = active_mem(place);

        ptr                   = world().call<mem::lea>(Defs{ptr, index});
        auto [new_mem, value] = world().call<mem::load>(Defs{mem, ptr})->projs<2>();
        rewritten_[tuple]     = world().tuple(Defs{mem, ptr});

        add_mem(place, new_mem);
        return rewritten_[extract] = value;
    }
    // If the tuple is not an array, we just return the original extract.
    return extract;
}

// const Def* Bufferize::remem() { return world().call<mem::remem>(mem_); }

} // namespace mim::plug::buffrize
