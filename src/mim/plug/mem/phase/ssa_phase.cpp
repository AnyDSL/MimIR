#include "mim/plug/mem/phase/ssa_phase.h"

#include <absl/container/fixed_array.h>

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

const Def* mem_store(const Def* mem, const Def* key, const Def* value) {
    auto& world    = mem->world();
    auto new_entry = world.tuple({key, value});
    if (auto proxy = mem->isa<Proxy>()) {
        auto new_map_entries = DefVec();
        new_map_entries.push_back(proxy->op(0));
        for (auto kv : proxy->ops() | std::views::drop(1)) {
            auto [k, v] = kv->projs<2>();
            if (k != key) new_map_entries.push_back(kv);
        }
        new_map_entries.push_back(new_entry);
        return world.proxy(world.annex<mem::M>(), new_map_entries, 0, 0);
    }
    return world.proxy(world.annex<mem::M>(), {mem, new_entry}, 0, 0);
}

std::pair<const Def*, const Def*> mem_load(const Def* mem, const Def* key) {
    if (auto proxy = mem->isa<Proxy>()) {
        for (auto kv : proxy->ops() | std::views::drop(1)) {
            auto [k, v] = kv->projs<2>();
            if (k == key) return {proxy->op(0), v};
        }
    }
    return {mem, nullptr};
}

const App* get_slot(const Def* ptr) {
    if (auto extract = ptr->isa<Extract>()) return Axm::isa<mem::slot>(extract->tuple());
    return nullptr;
}

const Def* SSAPhase::Analysis::rewrite_imm_App(const App* app) {
    if (auto load = Axm::isa<mem::load>(app)) {
        auto [mem, ptr] = load->args<2>();
        if (auto slot = get_slot(ptr)) {
            auto id  = slot->arg(1);
            auto map = rewrite(mem);
            if (auto [_, value] = mem_load(map, id); value) return world().tuple({map, value});
        }
    } else if (auto store = Axm::isa<mem::store>(app)) {
        auto [mem, ptr, val] = store->args<3>();
        if (auto slot = get_slot(ptr)) {
            auto map       = rewrite(mem);
            auto id        = slot->arg(1);
            auto abstr_val = rewrite(val);
            return mem_store(map, id, abstr_val);
        }
    }

    return SCCP::Analysis::rewrite_imm_App(app);
}

const Def* SSAPhase::rewrite_imm_App(const App* old_app) {
    if (auto load = Axm::isa<mem::load>(old_app)) {
        auto [mem, ptr] = load->args<2>();
        if (auto slot = get_slot(ptr)) {
            auto id  = slot->arg(1);
            auto map = lattice(mem);
            if (auto [mem, value] = mem_load(map, id); value) return new_world().tuple({rewrite(mem), rewrite(value)});
        }
    } else if (auto store = Axm::isa<mem::store>(old_app)) {
        // auto [mem, ptr, val] = store->args<3>();
        // if (auto slot = get_slot(ptr)) {
        //     auto map = concr2abstr(mem);
        //     auto id = slot->arg(1);
        //     auto abstr_val = concr2abstr(val);
        //     return map_insert(map, id, abstr_val);
        // }
    }

    return SCCP::rewrite_imm_App(old_app);
}

} // namespace mim::plug::mem::phase
