#include "dialects/clos/phase/higher_order_scalerize.h"

#include <functional>

#include "thorin/check.h"

namespace thorin::clos {

using DefQueue = std::deque<const Def*>;

void HigherOrderScalerize::start() {
    auto externals = std::vector(world().externals().begin(), world().externals().end());
    for (auto [_, n] : externals) {
        rewrite(n);
    }

    while (!worklist_.empty()) {
        auto lam = worklist_.top();
        worklist_.pop();
        if (lam->is_set()) {
            auto app = lam->body()->as<App>();
            auto callee = app->op(0);
            auto arg = app->op(1);
            lam->set(callee->reduce(arg));
        }
    }
}

const Def* HigherOrderScalerize::rewrite(const Def* def) {
    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;
    auto new_def = rewrite_convert(def);
    old2new_[def] = new_def;
    return new_def;
} 

bool schould_scalerize(const Pi* lam){
    for( auto dom :lam->doms() ){
        if(dom->isa<Sigma>()){
            return true;
        }else if( auto pi = dom->isa<Pi>() ){
            if(schould_scalerize(pi)){
                return true;
            }
        }
    }

    return false;
}

const Def* scalerize_ty(const Def* ty);
void aggregate_sigma(const Def* ty, DefQueue& ops){
    if(auto sigma = ty->isa<Sigma>()){
        for( auto op : sigma->ops() ){
            aggregate_sigma(op, ops);
        }
        return;
    }
    
    auto new_ty = scalerize_ty(ty);
    if(match<mem::M>(new_ty)){
        ops.push_front(new_ty);
    }else{
        ops.push_back(new_ty);
    }

    return;
}

const Def* scalerize_ty(const Def* ty){
    auto& w = ty->world();
    if(auto arr = ty->isa<Arr>()){
        auto new_body = scalerize_ty(arr->body());
        auto shape = arr->shape();
        return w.arr(shape, new_body);
    }else if(auto cn = ty->isa<Pi>()){
        auto dom = scalerize_ty(cn->dom());
        auto codom = scalerize_ty(cn->codom());
        return w.pi(dom, codom);
    }else if(auto sigma = ty->isa<Sigma>()){
        DefQueue ops;
        aggregate_sigma(sigma, ops);
        return w.sigma(DefArray(ops.begin(), ops.end()));
    }

    return ty;
}

void lam_vars(const Def* def, std::function<void(const Def*)> collector){
    if(def->num_projs() > 1){
        for( auto proj : def->projs() ){
            lam_vars(proj, collector);
        }

        return;
    }

    collector(def);
}

void flatten(const Def* def, DefVec& vars){
    lam_vars(def, [&](const Def* def){ vars.push_back(def);});
}

void flatten(const Def* def, DefQueue& vars){
    lam_vars(def, [&](const Def* def){ 
        if(match<mem::M>(def->type())){
            vars.push_front(def);
        }else{
            vars.push_back(def);
        }
    });
}

const Def* make_scalar_inv(const Def* def, const Def* target_ty) {
    if(!def->type()->isa<Pi>() || !target_ty->isa<Pi>()) return def;
    
    auto& w = def->world();
    auto wrapper = w.nom_lam(target_ty->as<Pi>(), def->dbg());

    DefQueue queue;
    flatten(wrapper->var(), queue);

    auto arg = w.tuple(DefArray(queue.begin(), queue.end()));

    wrapper->set_body(w.app(def, arg));
    wrapper->set_filter(true);
    return wrapper;
}

const Def* reshape(const Def* mem, const Def* ty, DefQueue& vars){
    if(ty->isa<Sigma>() || ty->isa<Arr>()){
        auto& w = ty->world();
        auto ops = DefArray(ty->num_ops(), [&](auto i) { return reshape(mem, ty->op(i), vars); });
        return w.tuple(ty, ops);
    }

    if( match<mem::M>(ty) ){
        return mem;
    }

    const Def* next = nullptr;

    assert(vars.size() != 0);
    next = vars.front();
    vars.pop_front();

    if( ty->isa<Pi>() && next->type() != ty ){
        next = make_scalar_inv(next, ty);
    }

    return next;
}

const Def* fill_extract_mem(const Def* def, DefQueue& vars){
    const Def* mem = nullptr;
    lam_vars(def, [&](const Def* def){ 
        if(match<mem::M>(def->type())){
            mem = def;
        }else{
            vars.push_back(def);
        }
    });
    return mem;
}

const Def* reshape(const Def* arg, const Pi* target_pi){
    DefQueue queue;
    const Def* mem = fill_extract_mem(arg, queue);
    auto target_arg = reshape(mem, target_pi->dom(), queue);
    assert(queue.empty());
    return target_arg;
}

const Def* HigherOrderScalerize::make_scalar(const Def* def) {
    auto pi_ty = def->type()->isa<Pi>();
    if(!pi_ty || !schould_scalerize(pi_ty)) return def;

    auto& w = def->world();

    auto new_ty = scalerize_ty(pi_ty)->as<Pi>();
    auto wrapper = w.nom_lam(new_ty, def->dbg());
    auto arg = reshape(wrapper->var(), pi_ty);

    wrapper->set_body(w.app(def, arg));
    wrapper->set_filter(true);
    worklist_.push(wrapper);
    return wrapper;
}

const Def* HigherOrderScalerize::rewrite_convert(const Def* def){
    switch (def->node()) {
        case Node::Bot:
        case Node::Top:
        case Node::Type:
        case Node::Univ:
        case Node::Nat: return def;
    }

    if(def->isa<Var>() || def->isa<Axiom>()){
        return def;
    }

    auto& w = world();

    auto new_type = rewrite(def->type());
    auto new_dbg  = def->dbg() ? rewrite(def->dbg()) : nullptr;


    if(auto malloc = match<mem::malloc>(def)){
        auto [mem, size] = rewrite(malloc->arg())->projs<2>();
        auto type = malloc->decurry()->arg(0);

        auto new_type = scalerize_ty(type);

        return mem::op_malloc(new_type, mem, malloc->dbg());
    }else if(auto lea = match<mem::lea>(def)){
        auto [ptr, idx] = rewrite(lea->arg())->projs<2>();

        return mem::op_lea(ptr, idx);
    }else if(auto store = match<mem::store>(def)){
        auto [mem, ptr, val] = rewrite(store->arg())->projs<3>();
        ptr->type()->dump();

        auto new_val = make_scalar(val);

        return mem::op_store(mem, ptr, new_val);
    }else if(auto load = match<mem::load>(def)){
        auto [rewritten_mem, rewritten_ptr] = rewrite(load->arg())->projs<2>();
        auto ptr_ty = match<mem::Ptr>(load->arg(1)->type())->arg(0);

        auto [mem, val] = mem::op_load(rewritten_mem, rewritten_ptr)->projs<2>();
        auto new_val = make_scalar_inv(val, ptr_ty);
        return w.tuple({mem, new_val});
    } else if(auto app = def->isa<App>()){

        auto callee = rewrite(app->op(0));
        auto arg = rewrite(app->op(1));

        if(!callee->isa<Axiom>()){
            arg = reshape(arg, callee->type()->as<Pi>());
        }

        auto new_app = w.app(callee, arg);

        return new_app;
    } else  if( auto lam = def->isa_nom<Lam>() ){
        Lam* new_lam = lam;
        bool do_scalerize = schould_scalerize(lam->type());
        if(do_scalerize){
            new_lam = make_scalar(lam)->as_nom<Lam>();
        }

        old2new_[def] = new_lam;

        if(lam->is_set()){
            auto new_body = rewrite(lam->body());
            lam->set_body(new_body);

        }

        new_lam->dump();
        return new_lam;
    } else {
        auto new_ops = DefArray(def->num_ops(), [&](auto i) { 
            return rewrite(def->op(i)); 
        });

        auto new_def = def->rebuild(w, new_type, new_ops, new_dbg);

        return new_def;
    }
}

}