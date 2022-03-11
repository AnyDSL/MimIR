#include "thorin/check.h"
#include "thorin/transform/closure_conv.h"
#include "thorin/analyses/scope.h"

namespace thorin {

/* auxillary functions */

// Adjust the index of an argument to account for the env param
size_t shift_env(size_t i) {
    return (i < CLOSURE_ENV_PARAM) ? i : i - 1_u64;
}

// Same but skip the env param
size_t skip_env(size_t i) {
    return (i < CLOSURE_ENV_PARAM) ? i : i + 1_u64;
}

const Def* closure_insert_env(size_t i, const Def* env, std::function<const Def* (size_t)> f) {
    return (i == CLOSURE_ENV_PARAM) ? env : f(shift_env(i));
}

const Def* closure_remove_env(size_t i, std::function<const Def* (size_t)> f) {
    return f(skip_env(i));
}

static const Def* ctype(World& w, Defs doms, const Def* env_type = nullptr) {
    if (!env_type) {
        auto sigma = w.nom_sigma(w.kind(), 3_u64, w.dbg("closure_type"));
        sigma->set(0_u64, w.kind());
        sigma->set(1_u64, ctype(w, doms, sigma->var(0_u64)));
        sigma->set(2_u64, sigma->var(0_u64));
        return sigma;
    } 
    return w.cn(DefArray(doms.size() + 1, [&](auto i) {
        return closure_insert_env(i, env_type, [&](auto j) { return doms[j]; });
    }));
}

Sigma* ctype(const Pi* pi) {
    return ctype(pi->world(), pi->doms(), nullptr)->as_nom<Sigma>();
}

const Pi* ctype_to_pi(const Def* ct, const Def* new_env_type) {
    assert(isa_ctype(ct));
    auto& w = ct->world();
    auto pi = ct->op(1_u64)->isa<Pi>();
    assert(pi);
    auto new_dom = new_env_type ? closure_sub_env(pi->dom(), new_env_type)
        : closure_remove_env(pi->dom());
    return w.cn(new_dom);
}

const Sigma* isa_ctype(const Def* def) {
    auto& w = def->world();
    auto sig = def->isa_nom<Sigma>();
    if (!sig || sig->num_ops() < 3 || sig->op(0_u64) != w.kind())
        return nullptr;
    auto var = sig->var(0_u64);
    if (sig->op(2_u64) != var)
        return nullptr;
    auto pi = sig->op(1_u64)->isa<Pi>();
    return (pi && pi->is_cn() && pi->num_ops() > 1_u64 && pi->dom(CLOSURE_ENV_PARAM) == var)
        ? sig : nullptr;
}

const Def* pack_closure_dbg(const Def* env, const Def* lam, const Def* dbg, const Def* ct) {
    assert(env && lam);
    assert(!ct || isa_ctype(ct));
    auto& w = env->world();
    auto pi = lam->type()->isa<Pi>();
    assert(pi && env->type() == pi->dom(CLOSURE_ENV_PARAM));
    ct = (ct) ? ct : ctype(w.cn(closure_remove_env(pi->dom())));
    return w.tuple(ct, {env->type(), lam, env}, dbg)->isa<Tuple>();
}

std::tuple<const Def*, const Def*, const Def*> unpack_closure(const Def* c) {
    assert(c && isa_ctype(c->type()));
    auto& w = c->world();
    auto env_type = c->proj(0_u64);
    auto pi = ctype_to_pi(c->type(), env_type);
    auto fn = w.extract_(pi, c, w.lit_int(3, 1));
    auto env = w.extract_(env_type, c, w.lit_int(3, 2));
    return {env_type, fn, env};
}

const Def* apply_closure(const Def* closure, const Def* args) {
    auto& w = closure->world();
    auto [_, fn, env] = unpack_closure(closure);
    auto pi = fn->type()->as<Pi>();
    return w.app(fn, DefArray(pi->num_doms(), [&](auto i) {
        return closure_insert_env(i, env, args);
    }));
}

ClosureLit isa_closure_lit(const Def* def, bool lambda_or_branch) {
    auto tpl = def->isa<Tuple>();
    if (tpl && isa_ctype(def->type())) {
        auto cc = CConv::bot;
        auto fnc = std::get<1_u64>(unpack_closure(tpl));
        if (auto q = isa<Tag::CConv>(fnc)) {
            fnc = q->arg();
            cc = q.flags();
        }
        if (!lambda_or_branch || fnc->isa<Lam>())
            return ClosureLit(tpl, cc);
    }
    return ClosureLit(nullptr, CConv::bot);
}

const Def* ClosureLit::env() {
    assert(def_);
    return std::get<2_u64>(unpack_closure(def_));
}

const Def* ClosureLit::fnc() {
    assert(def_);
    return std::get<1_u64>(unpack_closure(def_));
}

Lam* ClosureLit::fnc_as_lam() {
    auto f = fnc();
    if (auto q = isa<Tag::CConv>(f))
        f = q->arg();
    return f->isa_nom<Lam>();
}

const Def* ClosureLit::env_var() {
    return fnc_as_lam()->var(CLOSURE_ENV_PARAM);
}

unsigned int ClosureLit::order() { 
    return ctype_to_pi(type())->order(); 
}

/* Closure Conversion */

void ClosureConv::run() {
    auto& w = world();
    auto externals = std::vector(w.externals().begin(), w.externals().end());
    auto subst = Def2Def();
    w.DLOG("===== ClosureConv: start =====");
    for (auto [_, ext_def]: externals) {
        rewrite(ext_def, subst);
    }
    while (!worklist_.empty()) {
        auto def = worklist_.front();
        subst = Def2Def();
        worklist_.pop();
        if (auto closure = closures_.lookup(def)) {
            rewrite_body(closure->fn, subst);
        } else {
            w.DLOG("RUN: rewrite def {}", def);
            rewrite(def, subst);
        }
    }
    w.DLOG("===== ClosureConv: done ======");
}

void ClosureConv::rewrite_body(Lam* new_lam, Def2Def& subst) {
    auto& w = world();
    auto stub = closures_.lookup(new_lam);
    assert(stub && "closure should have a stub if rewrite_body is called!");
    auto [old_fn, num_fvs, env, new_fn] = *stub;

    if (!old_fn->is_set()) return;

    w.DLOG("rw body: {} [old={}, env={}]\nt", new_fn, old_fn, env);
    auto env_param = new_fn->var(CLOSURE_ENV_PARAM, w.dbg("closure_env"));
    if (num_fvs == 1) {
        subst.emplace(env, env_param);
    } else {
        for (size_t i = 0; i < num_fvs; i++) {
            auto fv = env->op(i);
            auto dbg = (fv->name().empty()) 
                ? w.dbg("fv_" + std::to_string(i))
                : w.dbg("fv_" + env->op(i)->name());
            subst.emplace(fv, env_param->proj(i, dbg));
        }
    }

    auto params =
        w.tuple(DefArray(old_fn->num_doms(), [&] (auto i) {
            return new_lam->var(skip_env(i), old_fn->var(i)->dbg());
        }));
    subst.emplace(old_fn->var(), params);

    auto filter = rewrite(new_fn->filter(), subst);
    auto body   = rewrite(new_fn->body(), subst);
    new_fn->set(filter, body);
}

const Def* ClosureConv::rewrite(const Def* def, Def2Def& subst) {
    switch(def->node()) {
        case Node::Kind:
        case Node::Space:
        case Node::Nat:
        case Node::Bot: // TODO This is used by the AD stuff????
        case Node::Top:
            return def;
        default:
            break;
    }

    auto& w = world();
    auto map = [&](const Def* new_def) {
        subst[def] = new_def;
        assert(subst[def] == new_def);
        return new_def;
    };

    if (auto new_def = subst.lookup(def)) {
        return *new_def;
    } else if (auto pi = def->isa<Pi>(); pi && pi->is_cn()) {
        return map(closure_type(pi, subst));
    } else if (auto lam = def->isa_nom<Lam>(); lam && lam->type()->is_cn()) {
        auto& w = world();
        auto [_, __, fv_env, new_lam] = make_stub(lam, subst);
        auto closure_type = rewrite(lam->type(), subst);
        auto env = rewrite(fv_env, subst);
        auto closure = pack_closure(env, new_lam, closure_type);
        w.DLOG("RW: pack {} ~> {} : {}", lam, closure, closure_type);
        return map(closure);
    } else if (auto q = isa<Tag::CConv>(CConv::ret, def)) {
        if (auto ret_lam = q->arg()->isa_nom<Lam>()) {
            assert(ret_lam && ret_lam->is_basicblock());
            // Note: This should be cont_lam's only occurance after η-expansion, so its okay to 
            // put into the local subst only
            auto new_doms = DefArray(ret_lam->num_doms(), [&](auto i) {
                    return rewrite(ret_lam->dom(i), subst);
            });
            auto new_lam = ret_lam->stub(w, w.cn(new_doms), ret_lam->dbg());
            subst[ret_lam] = new_lam;
            if (ret_lam->is_set()) {
                new_lam->set_filter(rewrite(ret_lam->filter(), subst));
                new_lam->set_body(rewrite(ret_lam->body(), subst));
            }
            return new_lam;
        }
    } else if (auto q = isa<Tag::CConv>(def); q && (q.flags() == CConv::fstclassBB || q.flags() == CConv::freeBB)) {
        // Note: Same thing about η-conversion applies here
        auto bb_lam = q->arg()->isa_nom<Lam>();
        assert(bb_lam && bb_lam->is_basicblock());
        auto [_, __, ___, new_lam] = make_stub({}, bb_lam, subst);
        subst[bb_lam] = pack_closure(w.tuple(), new_lam, rewrite(bb_lam->type(), subst));
        rewrite_body(new_lam, subst);
        return map(subst[bb_lam]);
    } else if (auto [var, lam] = ca_isa_var<Lam>(def); var && lam && lam->ret_var() == var) {
        // HACK to rewrite a retvar that is defined in an enclosing lambda
        // If we put external bb's into the env, this should never happen
        auto new_lam = make_stub(lam, subst).fn;
        auto new_idx = skip_env(as_lit(var->index()));
        return map(new_lam->var(new_idx));
    }

    auto new_type = rewrite(def->type(), subst);
    auto new_dbg = (def->dbg()) ? rewrite(def->dbg(), subst) : nullptr;

    if (auto nom = def->isa_nom()) {
        assert(!isa_ctype(nom));
        w.DLOG("RW: nom {}", nom);
        auto new_nom = nom->stub(w, new_type, new_dbg);
        subst.emplace(nom, new_nom);
        for (size_t i = 0; i < nom->num_ops(); i++) {
            if (def->op(i))
                new_nom->set(i, rewrite(def->op(i), subst));
        }
        if (Checker(w).equiv(nom, new_nom))
            return map(nom);
        if (auto restruct = new_nom->restructure())
            return map(restruct);
        return map(new_nom);
    } else {
        auto new_ops = DefArray(def->num_ops(), [&](auto i) {
            return rewrite(def->op(i), subst);
        });
        if (auto app = def->isa<App>(); app && new_ops[0]->type()->isa<Sigma>())
            return map(apply_closure(new_ops[0], new_ops[1]));
        else 
            return map(def->rebuild(w, new_type, new_ops, new_dbg));
    }
}

const Pi* ClosureConv::rewrite_cont_type(const Pi* pi, Def2Def& subst) {
    assert(pi->is_basicblock());
    auto new_ops = DefArray(pi->num_doms(), [&](auto i) { return rewrite(pi->dom(i), subst); });
    return world().cn(new_ops);
}

const Def* ClosureConv::closure_type(const Pi* pi, Def2Def& subst, const Def* env_type) {
    if (auto pct = closure_types_.lookup(pi); pct && !env_type)
        return *pct;
    auto& w = world();
    auto new_doms = DefArray(pi->num_doms(), [&](auto i) {
        return (i == pi->num_doms() - 1 && pi->is_returning()) ? rewrite_cont_type(pi->ret_pi(), subst) : rewrite(pi->dom(i), subst);
    });
    auto ct = ctype(w, new_doms, env_type);
    if (!env_type) {
        closure_types_.emplace(pi, ct);
        w.DLOG("C-TYPE: pct {} ~~> {}", pi, ct);
    } else {
        w.DLOG("C-TYPE: ct {}, env = {} ~~> {}", pi, env_type, ct);
    }
    return ct;
}

ClosureConv::ClosureStub ClosureConv::make_stub(const DefSet& fvs, Lam* old_lam, Def2Def& subst) {
    auto& w = world();
    auto env = w.tuple(DefArray(fvs.begin(), fvs.end()));
    auto num_fvs = fvs.size();
    auto env_type = rewrite(env->type(), subst);
    auto new_fn_type = closure_type(old_lam->type(), subst, env_type)->as<Pi>();
    auto new_lam = old_lam->stub(w, new_fn_type, w.dbg(old_lam->name()));
    new_lam->set_name((old_lam->is_external() || !old_lam->is_set())? "cc_" + old_lam->name() : old_lam->name());
    new_lam->set_body(old_lam->body());
    new_lam->set_filter(old_lam->filter());
    if (!isa_workable(old_lam)) {
        auto new_ext_type = w.cn(closure_remove_env(new_fn_type->dom()));
        auto new_ext_lam = old_lam->stub(w, new_ext_type, w.dbg(old_lam->name()));
        w.DLOG("wrap ext lam: {} -> stub: {}, ext: {}", old_lam, new_lam, new_ext_lam);
        if (old_lam->is_set()) {
            old_lam->make_internal();
            new_ext_lam->make_external();
            auto args = closure_insert_env(env, new_ext_lam->var());
            new_ext_lam->app(new_lam, args);
        } else {
            new_ext_lam->set(nullptr, nullptr);
            new_lam->app(new_ext_lam, closure_remove_env(new_lam->var()));
        }
    }
    w.DLOG("STUB {} ~~> ({}, {})", old_lam, env, new_lam);
    auto closure = ClosureStub{old_lam, num_fvs, env, new_lam};
    closures_.emplace(old_lam, closure);
    closures_.emplace(closure.fn, closure);
    return closure;
}

ClosureConv::ClosureStub ClosureConv::make_stub(Lam* old_lam, Def2Def& subst) {
    if (auto closure = closures_.lookup(old_lam))
        return *closure;
    auto closure = make_stub(fva_.run(old_lam), old_lam, subst);
    worklist_.emplace(closure.fn);
    return closure;
}

/* Free variable analysis */

void FVA::split_fv(Node* node, const Def* fv, bool& init_node, NodeQueue& worklist) {
    if (auto [var, lam] = ca_isa_var<Lam>(fv); var && lam) {
        if (var != lam->ret_var())
            node->fvs.emplace(fv);
        return;
    }
    if (auto q = isa<Tag::CConv>(CConv::freeBB, fv)) {
        node->fvs.emplace(q);
        return;
    }
    if (fv->no_dep() || fv->isa<Global>() || fv->isa<Axiom>())
        return;
    if (auto pred = fv->isa_nom()) {
        if (pred != node->nom) {
            auto [pnode, inserted] = build_node(pred, worklist);
            node->preds.push_back(pnode);
            pnode->succs.push_back(node);
            init_node |= inserted;
        }
    } else if (fv->dep() == Dep::Var && !fv->isa<Tuple>()) {
        // Var's can still have Def::Top, if their type is a nom!
        // So the first case is *not* redundant
        node->fvs.emplace(fv);
    } else {
        for (auto op: fv->ops())
            split_fv(node, op, init_node, worklist);
    }
}

std::pair<FVA::Node*, bool> FVA::build_node(Def *nom, NodeQueue& worklist) {
    auto& w = world();
    auto [p, inserted] = lam2nodes_.emplace(nom, nullptr);
    if (!inserted)
        return {p->second.get(), false};
    w.DLOG("FVA: create node: {}", nom);
    p->second = std::make_unique<Node>(Node{nom, {}, {}, {}, 0});
    auto node = p->second.get();
    auto scope = Scope(nom);
    bool init_node = false;
    for (auto v: scope.free_defs()) {
        split_fv(node, v, init_node, worklist);
    }
    if (!init_node) {
        worklist.push(node);
        w.DLOG("FVA: init {}", nom);
    }
    return {node, true};
}

void FVA::run(NodeQueue& worklist) {
    // auto& w = world();
    int iter = 0;
    while(!worklist.empty()) {
        auto node = worklist.front();
        worklist.pop();
        // w.DLOG("FA: iter {}: {}", iter, node->nom);
        if (is_done(node))
            continue;
        auto changed = is_bot(node);
        mark(node);
        for (auto p: node->preds) {
            auto& pfvs = p->fvs;
            changed |= node->fvs.insert(pfvs.begin(), pfvs.end());
            // w.DLOG("\tFV({}) ∪= FV({}) = {{{, }}}\b", node->nom, p->nom, pfvs);
        }
        if (changed) {
            for (auto s: node->succs) {
                worklist.push(s);
            }
        }
        iter++;
    }
    // w.DLOG("FVA: done");
}

DefSet& FVA::run(Lam *lam) {
    auto worklist = NodeQueue();
    auto [node, _] = build_node(lam, worklist);
    if (!is_done(node)) {
        cur_pass_id++;
        run(worklist);
    }
    return node->fvs;
}

} // namespace thorin
