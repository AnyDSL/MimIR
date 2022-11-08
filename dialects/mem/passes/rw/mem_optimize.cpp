#include "dialects/mem/passes/rw/mem_optimize.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* MemOptimize::optimize_load(const Def* mem, const Def* ptr) {
    if (auto extract = mem->isa<Extract>()) {
        auto tuple = extract->tuple();

        if (auto load = match<mem::load>(tuple)) {
            auto [mem, other_ptr] = load->args<2>();

            if (other_ptr == ptr) {
                return load->proj(1);
            } else {
                return optimize_load(mem, ptr);
            }
        } else if (auto store = match<mem::store>(tuple)) {
            auto [mem, other_ptr, val] = store->args<3>();

            if (other_ptr == ptr) { return val; }
        } else if (auto app = tuple->isa<App>()) {
            auto prev_mem = mem::mem_def(app->arg());
            force<mem::M>(prev_mem->type());
            return optimize_load(prev_mem, ptr);
        }
    }

    return nullptr;
}

const Def* MemOptimize::optimize_store(const Def* mem, const Def* ptr) {
    auto& w = world();
    if (auto extract = mem->isa<Extract>()) {
        auto tuple = extract->tuple();

        if (auto store = match<mem::store>(tuple)) {
            auto [mem, other_ptr, val] = store->args<3>();

            if (other_ptr == ptr) {
                return store->proj(0);
            } else {
                auto new_mem = optimize_store(mem, ptr);
                return w.app(store->callee(), {new_mem, other_ptr, val});
            }
        }
    }

    return mem;
}

const Def* MemOptimize::rewrite(const Def* def) {
    auto& w = world();

    if (auto load = match<mem::load>(def)) {
        auto [mem, ptr] = load->args<2>();
        if (auto new_val = optimize_load(mem, ptr)) { return w.tuple({mem, new_val}); }
    } else if (auto store = match<mem::store>(def)) {
        auto [mem, ptr, val] = store->args<3>();
        if (auto new_mem = optimize_store(mem, ptr)) { return w.app(store->callee(), {new_mem, ptr, val}); }
    }

    return def;
}

} // namespace thorin::autodiff
