#include "thorin/world.h"

// for colored output
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/error.h"
#include "thorin/normalize.h"
#include "thorin/rewrite.h"
#include "thorin/tables.h"
#include "thorin/analyses/scope.h"
#include "thorin/util/array.h"
#include "thorin/util/container.h"

namespace thorin {

/*
 * constructor & destructor
 */

#if (!defined(_MSC_VER) && defined(NDEBUG))
bool World::Arena::Lock::guard_ = false;
#endif

World::World(std::string_view name)
    : checker_(std::make_unique<Checker>(*this))
{
    data_.name_     = name.empty() ? "module" : name;
    data_.space_    = insert<Space>(0, *this);
    data_.kind_     = insert<Kind>(0, *this);
    data_.bot_kind_ = insert<Bot>(0, kind(), nullptr);
    data_.sigma_    = insert<Sigma>(0, kind(), Defs{}, nullptr)->as<Sigma>();
    data_.tuple_    = insert<Tuple>(0, sigma(), Defs{}, nullptr)->as<Tuple>();
    data_.type_nat_ = insert<Nat>(0, *this);
    data_.top_nat_  = insert<Top>(0, type_nat(), nullptr);
    data_.lit_nat_0_   = lit_nat(0);
    data_.lit_nat_1_   = lit_nat(1);
    data_.lit_nat_max_ = lit_nat(nat_t(-1));
    auto nat = type_nat();

    {   // int/real: w: Nat -> *
        auto p = pi(nat, kind());
        data_.type_int_     = axiom(p, Tag:: Int, 0);
        data_.type_real_    = axiom(p, Tag::Real, 0);
        data_.type_bool_    = type_int(2);
        data_.lit_bool_[0]  = lit_int(2, 0_u64);
        data_.lit_bool_[1]  = lit_int(2, 1_u64);
    }

    auto mem = data_.type_mem_ = axiom(kind(), Tag::Mem, 0, dbg("mem"));

    { // ptr: [T: *, as: nat] -> *
        data_.type_ptr_ = axiom(nullptr, pi({kind(), nat}, kind()), Tag::Ptr, 0, dbg("ptr"));
    } {
#define CODE(T, o) data_.T ## _[size_t(T::o)] = axiom(normalize_ ## T<T::o>, type, Tag::T, flags_t(T::o), dbg(op2str(T::o)));
    } { // bit: w: nat -> [int w, int w] -> int w
        auto type = nom_pi(kind())->set_dom(nat);
        auto int_w = type_int(type->var(dbg("w")));
        type->set_codom(pi({int_w, int_w}, int_w));
        THORIN_BIT(CODE)
    } { // Shr: w: nat -> [int w, int w] -> int w
        auto type = nom_pi(kind())->set_dom(nat);
        auto int_w = type_int(type->var(dbg("w")));
        type->set_codom(pi({int_w, int_w}, int_w));
        THORIN_SHR(CODE)
    } { // Wrap: [m: nat, w: nat] -> [int w, int w] -> int w
        auto type = nom_pi(kind())->set_dom({nat, nat});
        auto [m, w] = type->vars<2>({dbg("m"), dbg("w")});
        auto int_w = type_int(w);
        type->set_codom(pi({int_w, int_w}, int_w));
        THORIN_WRAP(CODE)
    } { // Div: w: nat -> [mem, int w, int w] -> [mem, int w]
        auto type = nom_pi(kind())->set_dom(nat);
        auto int_w = type_int(type->var(dbg("w")));
        type->set_codom(pi({mem, int_w, int_w}, sigma({mem, int_w})));
        THORIN_DIV(CODE)
    } { // ROp: [m: nat, w: nat] -> [real w, real w] -> real w
        auto type = nom_pi(kind())->set_dom({nat, nat});
        auto [m, w] = type->vars<2>({dbg("m"), dbg("w")});
        auto real_w = type_real(w);
        type->set_codom(pi({real_w, real_w}, real_w));
        THORIN_R_OP(CODE)
    } { // ICmp: w: nat -> [int w, int w] -> bool
        auto type = nom_pi(kind())->set_dom(nat);
        auto int_w = type_int(type->var(dbg("w")));
        type->set_codom(pi({int_w, int_w}, type_bool()));
        THORIN_I_CMP(CODE)
    } { // RCmp: [m: nat, w: nat] -> [real w, real w] -> bool
        auto type = nom_pi(kind())->set_dom({nat, nat});
        auto [m, w] = type->vars<2>({dbg("m"), dbg("w")});
        auto real_w = type_real(w);
        type->set_codom(pi({real_w, real_w}, type_bool()));
        THORIN_R_CMP(CODE)
    } { // trait: T: * -> nat
        auto type = pi(kind(), nat);
        THORIN_TRAIT(CODE)
    } { // acc: n: nat -> cn[M, cn[M, int w n, cn[M, []]]]
        // TODO this is more a proof of concept
        auto type = nom_pi(kind())->set_dom(nat);
        auto n = type->var(0, dbg("n"));
        type->set_codom(cn_mem_ret(type_int(n), sigma()));
        THORIN_ACC(CODE)
    }
#undef CODE
    {   // Conv: [dw: nat, sw: nat] -> i/r sw -> i/r dw
        auto make_type = [&](Conv o) {
            auto type = nom_pi(kind())->set_dom({nat, nat});
            auto [dw, sw] = type->vars<2>({dbg("dw"), dbg("sw")});
            auto type_dw = o == Conv::s2r || o == Conv::u2r || o == Conv::r2r ? type_real(dw) : type_int(dw);
            auto type_sw = o == Conv::r2s || o == Conv::r2u || o == Conv::r2r ? type_real(sw) : type_int(sw);
            return type->set_codom(pi(type_sw, type_dw));
        };
#define CODE(T, o) data_.Conv_[size_t(T::o)] = axiom(normalize_Conv<T::o>, make_type(T::o), Tag::Conv, flags_t(T::o), dbg(op2str(T::o)));
        THORIN_CONV(CODE)
#undef Code
    } { // hlt/run: T: * -> T -> T
        auto type = nom_pi(kind())->set_dom(kind());
        auto T = type->var(dbg("T"));
        type->set_codom(pi(T, T));
        data_.PE_[size_t(PE::hlt)] = axiom(normalize_PE<PE::hlt>, type, Tag::PE, flags_t(PE::hlt), dbg(op2str(PE::hlt)));
        data_.PE_[size_t(PE::run)] = axiom(normalize_PE<PE::run>, type, Tag::PE, flags_t(PE::run), dbg(op2str(PE::run)));
    } { // known: T: * -> T -> bool
        auto type = nom_pi(kind())->set_dom(kind());
        auto T = type->var(dbg("T"));
        type->set_codom(pi(T, type_bool()));
        data_.PE_[size_t(PE::known)] = axiom(normalize_PE<PE::known>, type, Tag::PE, flags_t(PE::known), dbg(op2str(PE::known)));
    } { // bitcast: [D: *, S: *] -> S -> D
        auto type = nom_pi(kind())->set_dom({kind(), kind()});
        auto [D, S] = type->vars<2>({dbg("D"), dbg("S")});
        type->set_codom(pi(S, D));
        data_.bitcast_ = axiom(normalize_bitcast, type, Tag::Bitcast, 0, dbg("bitcast"));
    } { // lea:, [n: nat, Ts: «n; *», as: nat] -> [ptr(«j: n; Ts#j», as), i: int n] -> ptr(Ts#i, as)
        auto dom = nom_sigma(space(), 3);
        dom->set(0, nat);
        dom->set(1, arr(dom->var(0, dbg("n")), kind()));
        dom->set(2, nat);
        auto pi1 = nom_pi(kind())->set_dom(dom);
        auto [n, Ts, as] = pi1->vars<3>({dbg("n"), dbg("Ts"), dbg("as")});
        auto in = nom_arr(n);
        in->set(extract(Ts, in->var(dbg("j"))));
        auto pi2 = nom_pi(kind())->set_dom({type_ptr(in, as), type_int(n)});
        pi2->set_codom(type_ptr(extract(Ts, pi2->var(1, dbg("i"))), as));
        pi1->set_codom(pi2);
        data_.lea_ = axiom(normalize_lea, pi1, Tag::LEA, 0, dbg("lea"));
    } { // load: [T: *, as: nat] -> [M, ptr(T, as)] -> [M, T]
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi({mem, ptr}, sigma({mem, T})));
        data_.load_ = axiom(normalize_load, type, Tag::Load, 0, dbg("load"));
    } { // remem: M -> M
        auto type = pi(mem, mem);
        data_.remem_ = axiom(normalize_remem, type, Tag::Remem, 0, dbg("remem"));
    } { // store: [T: *, as: nat] -> [M, ptr(T, as), T] -> M
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi({mem, ptr, T}, mem));
        data_.store_ = axiom(normalize_store, type, Tag::Store, 0, dbg("store"));
    } { // alloc: [T: *, as: nat] -> M -> [M, ptr(T, as)]
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi(mem, sigma({mem, ptr})));
        data_.alloc_ = axiom(nullptr, type, Tag::Alloc, 0, dbg("alloc"));
    } { // slot: [T: *, as: nat] -> [M, nat] -> [M, ptr(T, as)]
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi({mem, nat}, sigma({mem, ptr})));
        data_.slot_ = axiom(nullptr, type, Tag::Slot, 0, dbg("slot"));
    } { // malloc: [T: *, as: nat] -> [M, nat] -> [M, ptr(T, as)]
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi({mem, nat}, sigma({mem, ptr})));
        data_.malloc_ = axiom(nullptr, type, Tag::Malloc, 0, dbg("malloc"));
    } { // mslot: [T: *, as: nat] -> [M, nat, nat] -> [M, ptr(T, as)]
        auto type = nom_pi(kind())->set_dom({kind(), nat});
        auto [T, as] = type->vars<2>({dbg("T"), dbg("as")});
        auto ptr = type_ptr(T, as);
        type->set_codom(pi({mem, nat, nat}, sigma({mem, ptr})));
        data_.mslot_ = axiom(nullptr, type, Tag::Mslot, 0, dbg("mslot"));
    } { // atomic: [T: *, R: *] -> T -> R
        auto type = nom_pi(kind())->set_dom({kind(), kind()});
        auto [T, R] = type->vars<2>({dbg("T"), dbg("R")});
        type->set_codom(pi(T, R));
        data_.atomic_ = axiom(nullptr, type, Tag::Atomic, 0, dbg("atomic"));
    } { // zip: [r: nat, s: «r; nat»] -> [n_i: nat, Is: «n_i; *», n_o: nat, Os: «n_o; *», f: «i: n_i; Is#i» -> «o: n_o; Os#o»] -> «i: n_i; «s; Is#i»» -> «o: n_o; «s; Os#o»»
        // TODO select which Is/Os to zip
        auto rs = nom_sigma(kind(), 2);
        rs->set(0, nat);
        rs->set(1, arr(rs->var(0, dbg("r")), nat));
        auto rs_pi = nom_pi(kind())->set_dom(rs);
        auto s = rs_pi->var(1, dbg("s"));

        // [n_i: nat, Is: «n_i; *», n_o: nat, Os: «n_o; *», f: «i: n_i; Is#i» -> «o: n_o; Os#o»,]
        auto is_os = nom_sigma(space(), 5);
        is_os->set(0, nat);
        is_os->set(1, arr(is_os->var(0, dbg("n_i")), kind()));
        is_os->set(2, nat);
        is_os->set(3, arr(is_os->var(2, dbg("n_o")), kind()));
        auto f_i = nom_arr(is_os->var(0_u64));
        auto f_o = nom_arr(is_os->var(2_u64));
        f_i->set(extract(is_os->var(1, dbg("Is")), f_i->var()));
        f_o->set(extract(is_os->var(3, dbg("Os")), f_o->var()));
        is_os->set(4, pi(f_i, f_o));
        auto is_os_pi = nom_pi(kind())->set_dom(is_os);

        // «i: n_i; «s; Is#i»» -> «o: n_o; «s; Os#o»»
        auto dom = nom_arr(is_os_pi->var(0_u64, dbg("n_i")));
        auto cod = nom_arr(is_os_pi->var(2_u64, dbg("n_o")));
        dom->set(arr(s, extract(is_os_pi->var(1, dbg("Is")), dom->var())));
        cod->set(arr(s, extract(is_os_pi->var(3, dbg("Os")), cod->var())));

        is_os_pi->set_codom(pi(dom, cod));
        rs_pi->set_codom(is_os_pi);

        data_.zip_ = axiom(normalize_zip, rs_pi, Tag::Zip, 0, dbg("zip"));
    } { // op_rev_diff: Π[I:*.O:*]. ΠI. O
        // DS: I can't figure out how to give it the correct type…
        // pullback assumes that:
        //     I = Π[mem, T₁, …, Tₙ, Π[mem, R₁, …, Rₙ].⊥].⊥
        //     O = Π[mem, T₁, …, Tₙ, Π[mem, R₁, …, Rₙ, Π[mem, R'₁, …, R'ₙ, ΠT'.⊥].⊥].⊥].⊥
        // where
        //     α' = type_tangent_vector(α)

        /*
        auto type = nom_pi(kind())->set_dom({ kind(), kind() });
        auto I = type->var(0, dbg("I"));
        auto O = type->var(1, dbg("O"));
        type->set_codom(pi(I, O));
        data_.op_rev_diff_ = axiom(nullptr, type, Tag::RevDiff, 0, dbg("rev_diff"));
        */
        // TODO: generalize this axiom for arbitrary functions
        // what we basically want is an operator that looks like this:
        //   A →  B →  (A →  B →  (A × B →  B × A))
        //              \---------- Ξ ------------/
        /*
        auto type = nom_pi(kind())->set_dom({kind(), kind()});
        auto A = type->var(0, dbg("A"));
        auto B = type->var(1, dbg("B"));

        auto diffd = cn({
          type_mem(),
          A,
          B,
          cn({type_mem(), sigma({B, A})})
        });
        auto Xi = pi(cn_mem_flat(A, B), diffd);
        type->set_codom(Xi);
        data_.op_rev_diff_ = axiom(nullptr, type, Tag::RevDiff, 0, dbg("rev_diff"));
        */
//        auto type = nom_pi(kind())->set_dom({kind(), kind(), kind(), kind(), kind(), kind()});
//        auto [A, B, C, D,E,F] = type->vars<6>({dbg("A"), dbg("B"),dbg("C"),dbg("D"),dbg("E"),dbg("F")});
//
//        auto pullback = cn_mem_ret(E,F);
//        auto diffd = cn({
//          type_mem(),
//          C,
////          flatten(A),
//          cn({type_mem(), D, pullback})
//        });
////        auto diffd= cn_mem_flat(A,tuple({B,pullback}));
//        // TODO: flattening at this point is useless as we handle abstract kinds here
//        auto Xi = pi(cn_mem_ret(A, B), diffd);
//        //        auto Xi = pi(cn_mem_ret(flatten(A), B), diffd);
////        auto Xi = pi(cn_mem_flat(A, B), diffd);
//        type->set_codom(Xi);
//        data_.op_rev_diff_ = axiom(nullptr, type, Tag::RevDiff, 0, dbg("rev_diff"));

        auto type = nom_pi(kind())->set_dom({kind(), kind()});
        auto [X,Y] = type->vars<2>({dbg("X"), dbg("Y")});

        auto Xi = pi(X,Y);
        type->set_codom(Xi);
        data_.op_rev_diff_ = axiom(nullptr, type, Tag::RevDiff, 0, dbg("rev_diff"));
    }
}


// reflect impala tangent type
const Def* World::tangent_type(const Def* A,bool left) {
    Stream s2;
    s2.fmt("A: {} : {}, {}\n",A,A->type(), A->node_name());

    if(auto pidef = A->isa<Pi>();pidef && left) {
        s2.fmt("A is pi\n");
//        s2.fmt("A exists?\n");
//
//        s2.fmt("V0 {}\n",pidef->dom(0));
//        s2.fmt("V1 {}\n",pidef->dom(1));
//        s2.fmt("V2 {}\n",pidef->dom(2)->as<Pi>()->dom(1));

//        s2.fmt("pidef {}\n ",pidef);
//        s2.fmt("ops {}\n ",pidef->num_ops());
//        s2.fmt("out {}\n ",pidef->num_outs());
//        s2.fmt("doms {}\n ",pidef->num_doms());
//        s2.fmt("codoms {}\n ",pidef->num_codoms());
        if(pidef->num_doms()==1) {
            //cn :mem
//            return pidef;
            return cn(tangent_type(pidef->dom(1),left));
            // or cn(type_mem) if mem
        }

        // TODO: multiple variables
        auto A = pidef->dom(1);
        auto B = pidef->dom(2)->as<Pi>()->dom(1);
        auto AL = tangent_type(A,true);
        auto BL = tangent_type(A,true);

        auto pullback = cn_mem_ret(tangent_type(B,false), tangent_type(A,false));
        auto diffd = cn({
                            type_mem(),
                            AL,
                            cn({type_mem(), BL, pullback})
                        });
//        auto diffd= cn_mem_flat(A,tuple({B,pullback}));

        return diffd;

//        THORIN_UNREACHABLE;

//        auto diffd = cn({
//                            type_mem(),
//                            A,
//                            cn({type_mem(), B, pullback})
//                        });
//        auto Xi = pi(cn_mem_ret(A, B), diffd);

//        auto dom = pidef->dom();
//        s2.fmt("dom {} \n",dom);
//        auto codom = pidef->codom();
//        s2.fmt("codom {} \n",codom);
//        return pi(tangent_type(codom), tangent_type(dom),pidef->dbg());
    }
    if(auto ptr = isa<Tag::Ptr>(A)) {
        s2.fmt("A is ptr\n");
        auto [pointee, addr_space] = ptr->arg()->projs<2>();
        auto inner=tangent_type(pointee,left);
//        return inner;
        if(pointee->isa<Arr>() || left) {
            s2.fmt("Ptr -> Arr\n");
            return type_ptr(inner,addr_space);
        }
        return inner;
    }
    if(auto arrdef = A->isa<Arr>()) {
//        s2.fmt("A is arr\n");
        return arr(arrdef->shape(), tangent_type(arrdef->body(),left),arrdef->dbg());
    }
    if(auto sig = A->isa<Sigma>()) {
        // TODO: handle structs
//        s2.fmt("A is Sigma\n");
//        s2.fmt("A fields {} \n",sig->fields());
//        s2.fmt("A is structural {} \n",sig->isa_structural());
        auto ops = sig->ops();
        Array<const Def*> tan_ops_arr{ops.size() ,[&](auto i) {
                return tangent_type(ops[i],left);
        }};
        Defs tan_ops{tan_ops_arr};
        return sigma(tan_ops,sig->dbg());
    }
    if(auto real = isa<Tag::Real>(A)) {
        return A;
    }else {
        // dummy deriv
       return left ? A : type_real(64);
    }
}



World::~World() {
    for (auto def : data_.defs_) def->~Def();
}

/*
 * core calculus
 */

const Def* World::app(const Def* callee, const Def* arg, const Def* dbg) {
    auto pi = callee->type()->as<Pi>();

    if (err()) {
        if (!checker_->assignable(pi->dom(), arg))
            err()->ill_typed_app(callee, arg);
    }

    auto type = pi->apply(arg).back();
    auto [axiom, currying_depth] = get_axiom(callee); // TODO move down again
    if (axiom && currying_depth == 1) {
//        if(axiom->tag()==Tag::Lift)
//            DLOG("Lift Axiom & CURRYING DEPTH");
        if (auto normalize = axiom->normalizer())
            return normalize(type, callee, arg, dbg);
    }

    return unify<App>(2, axiom, currying_depth-1, type, callee, arg, dbg);
}

const Def* World::raw_app(const Def* callee, const Def* arg, const Def* dbg) {
    auto pi = callee->type()->as<Pi>();
    auto type = pi->apply(arg).back();
    auto [axiom, currying_depth] = get_axiom(callee);
    return unify<App>(2, axiom, currying_depth-1, type, callee, arg, dbg);
}

const Def* World::sigma(Defs ops, const Def* dbg, bool flatten) {
    auto n = ops.size();

//    Stream s2;
//    s2.fmt("sigma [{, }] dbg: {}\n",ops,dbg);

    if (n == 0) return sigma();
    if (n == 1 && flatten) return ops[0];
    // or don't do it while flattening
    // n>1
    if (flatten && std::all_of(ops.begin()+1, ops.end(), [&](auto op) { return ops[0] == op; })) return arr(n, ops[0]);
    return unify<Sigma>(ops.size(), infer_kind(ops), ops, dbg);
}

static const Def* infer_sigma(World& world, Defs ops) {
    DefArray elems(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i)
        elems[i] = ops[i]->type();

    return world.sigma(elems);
}

const Pi* World::cn_mem_half_flat(const Def* dom, const Def* codom, const Def* dbg) {
    auto ret = cn(sigma({ type_mem(), codom }));

    if (dom->isa<Sigma>()) {
        auto size = dom->num_ops() + 2;
        DefArray defs(size);
        for (size_t i = 0; i < size; ++i) {
            if (i == 0) {
                defs[i] = type_mem();
            } else if (i == size - 1) {
                defs[i] = cn(ret);
            } else {
                defs[i] = dom->op(i);
            }
        }

        return cn(defs);
    }

    if (auto a = dom->isa<Arr>()) {
        auto size = a->shape()->as<Lit>()->get<uint8_t>() + 2;
        DefArray defs(size);
        for (uint8_t i = 0; i < size; ++i) {
            if (i == 0) {
                defs[i] = type_mem();
            } else if (i == size - 1) {
                defs[i] = ret;
            } else {
                defs[i] = a->body();
            }
        }

        return cn(defs);
    }

    return cn(merge(type_mem(), {dom, ret}), dbg);
}

const Pi* World::cn_mem_flat(const Def* dom, const Def* codom, const Def* dbg) {
    auto ret = cn(sigma({ type_mem(), codom }));
    if (codom->isa<Sigma>()) {
        ret = cn(merge_sigma(type_mem(), codom->ops())) ;
    }
    if (auto a = codom->isa<Arr>()) {
        auto size = a->shape()->as<Lit>()->get<uint8_t>() + 1;
        DefArray defs(size);
        for (uint8_t i = 0; i < size - 1; ++i) {
            defs[i + 1] = a->body();
        }
        defs.front() = type_mem();
        ret = cn(defs);
    }

    if (dom->isa<Sigma>()) {
        auto size = dom->num_ops() + 2;
        DefArray defs(size);
        for (size_t i = 0; i < size; ++i) {
            if (i == 0) {
                defs[i] = type_mem();
            } else if (i == size - 1) {
                defs[i] = ret;
            } else {
                defs[i] = dom->op(i - 1);
            }
        }

        return cn(defs);
    }

    if (auto a = dom->isa<Arr>()) {
        auto size = a->shape()->as<Lit>()->get<uint8_t>() + 2;
        DefArray defs(size);
        for (uint8_t i = 0; i < size; ++i) {
            if (i == 0) {
                defs[i] = type_mem();
            } else if (i == size - 1) {
                defs[i] = ret;
            } else {
                defs[i] = a->body();
            }
        }

        return cn(defs);
    }

    return cn(merge(type_mem(), {dom, ret}), dbg);
}


const Def* World::tuple(Defs ops, const Def* dbg) {
    auto sigma = infer_sigma(*this, ops);
    auto t = tuple(sigma, ops, dbg);
    if (err() && !checker_->assignable(sigma, t)) {
        assert(false && "TODO: error msg");
    }

    return t;
}

const Def* World::tuple(const Def* type, Defs ops, const Def* dbg) {
    if (err()) {
    // TODO type-check type vs inferred type
    }

    auto n = ops.size();
    if (!type->isa_nom<Sigma>()) {
        if (n == 0) return tuple();
        if (n == 1) return ops[0];
        if (std::all_of(ops.begin()+1, ops.end(), [&](auto op) { return ops[0] == op; })) return pack(n, ops[0]);
    }

    // eta rule for tuples:
    // (extract(tup, 0), extract(tup, 1), extract(tup, 2)) -> tup
    if (n != 0) if (auto extract = ops[0]->isa<Extract>()) {
        auto tup = extract->tuple();
        bool eta = tup->type() == type;
        for (size_t i = 0; i != n && eta; ++i) {
            if (auto extract = ops[i]->isa<Extract>()) {
                if (auto index = isa_lit(extract->index())) {
                    if (eta &= u64(i) == *index) {
                        eta &= extract->tuple() == tup;
                        continue;
                    }
                }
            }
            eta = false;
        }

        if (eta) return tup;
    }

    return unify<Tuple>(ops.size(), type, ops, dbg);
}

const Def* World::tuple_str(std::string_view s, const Def* dbg) {
    DefVec ops;
    for (auto c : s) ops.emplace_back(lit_nat(c));
    return tuple(ops, dbg);
}

const Def* World::extract_(const Def* ex_type, const Def* tup, const Def* index, const Def* dbg) {
    if (index->isa<Arr>() || index->isa<Pack>()) {
        DefArray ops(as_lit(index->arity()), [&](size_t) { return extract(tup, index->ops().back()); });
        return index->isa<Arr>() ? sigma(ops, dbg) : tuple(ops, dbg);
    } else if (index->isa<Sigma>() || index->isa<Tuple>()) {
        auto n = index->num_ops();
        DefArray idx(n, [&](size_t i) { return index->op(i); });
        DefArray ops(n, [&](size_t i) { return tup->proj(n, as_lit(idx[i])); });
        return index->isa<Sigma>() ? sigma(ops, dbg) : tuple(ops, dbg);
    }

    auto type = tup->type()->reduce();
    if (err()) {
        if (!checker_->equiv(type->arity(), isa_sized_type(index->type())))
            err()->index_out_of_range(type->arity(), index);
    }

    // nom sigmas can be 1-tuples
    if (auto mod = isa_lit(isa_sized_type(index->type())); mod && *mod == 1 && !tup->type()->isa_nom<Sigma>()) return tup;
    if (auto pack = tup->isa_structural<Pack>()) return pack->body();

    // extract(insert(x, index, val), index) -> val
    if (auto insert = tup->isa<Insert>()) {
        if (index == insert->index())
            return insert->value();
    }

    if (auto i = isa_lit(index)) {
        if (auto tuple = tup->isa<Tuple>()) return tuple->op(*i);

        // extract(insert(x, j, val), i) -> extract(x, i) where i != j (guaranteed by rule above)
        if (auto insert = tup->isa<Insert>()) {
            if (insert->index()->isa<Lit>())
                return extract(insert->tuple(), index, dbg);
        }

        if (type->isa<Sigma>())
            return unify<Extract>(2, ex_type ? ex_type : type->op(*i), tup, index, dbg);
    }

    type = type->as<Arr>()->body();
    return unify<Extract>(2, type, tup, index, dbg);
}

const Def* World::insert(const Def* tup, const Def* index, const Def* val, const Def* dbg) {
    auto type = tup->type()->reduce();

    if (err() && !checker_->equiv(type->arity(), isa_sized_type(index->type())))
        err()->index_out_of_range(type->arity(), index);

    if (auto mod = isa_lit(isa_sized_type(index->type())); mod && *mod == 1)
        return tuple(tup, {val}, dbg); // tup could be nom - that's why the tuple ctor is needed

    // insert((a, b, c, d), 2, x) -> (a, b, x, d)
    if (auto t = tup->isa<Tuple>()) return t->refine(as_lit(index), val);

    // insert(‹4; x›, 2, y) -> (x, x, y, x)
    if (auto pack = tup->isa<Pack>()) {
        if (auto a = isa_lit(pack->arity())) {
            DefArray new_ops(*a, pack->body());
            new_ops[as_lit(index)] = val;
            return tuple(type, new_ops, dbg);
        }
    }

    // insert(insert(x, index, y), index, val) -> insert(x, index, val)
    if (auto insert = tup->isa<Insert>()) {
        if (insert->index() == index)
            tup = insert->tuple();
    }

    return unify<Insert>(3, tup, index, val, dbg);
}

bool is_shape(const Def* s) {
    if (s->isa<Nat>()) return true;
    if (auto arr = s->isa<Arr  >()) return arr->body()->isa<Nat>();
    if (auto sig = s->isa_structural<Sigma>())
        return std::ranges::all_of(sig->ops(), [](const Def* op) { return op->isa<Nat>(); });

    return false;
}

const Def* World::arr(const Def* shape, const Def* body, const Def* dbg) {
    if (err()) {
        if (!is_shape(shape->type())) err()->expected_shape(shape);
    }

    if (auto a = isa_lit<u64>(shape)) {
        if (*a == 0) return sigma();
        if (*a == 1) return body;
    }

    // «(a, b, c); body» -> «a; «(b, c); body»»
    if (auto tuple = shape->isa<Tuple>())
        return arr(tuple->ops().front(), arr(tuple->ops().skip_front(), body), dbg);

    // «<n; x>; body» -> «x; «<n-1, x>; body»»
    if (auto p = shape->isa<Pack>()) {
        if (auto s = isa_lit(p->shape()))
            return arr(*s, arr(pack(*s-1, p->body()), body), dbg);
    }

    return unify<Arr>(2, kind(), shape, body, dbg);
}

const Def* World::pack(const Def* shape, const Def* body, const Def* dbg) {
    if (err()) {
        if (!is_shape(shape->type())) err()->expected_shape(shape);
    }

    if (auto a = isa_lit<u64>(shape)) {
        if (*a == 0) return tuple();
        if (*a == 1) return body;
    }

    // <(a, b, c); body> -> <a; «(b, c); body>>
    if (auto tuple = shape->isa<Tuple>())
        return pack(tuple->ops().front(), pack(tuple->ops().skip_front(), body), dbg);

    // <<n; x>; body> -> <x; <<n-1, x>; body>>
    if (auto p = shape->isa<Pack>()) {
        if (auto s = isa_lit(p->shape()))
            return pack(*s, pack(pack(*s-1, p->body()), body), dbg);
    }

    auto type = arr(shape, body->type());
    return unify<Pack>(1, type, body, dbg);
}

const Def* World::arr(Defs shape, const Def* body, const Def* dbg) {
    if (shape.empty()) return body;
    return arr(shape.skip_back(), arr(shape.back(), body, dbg), dbg);
}

const Def* World::pack(Defs shape, const Def* body, const Def* dbg) {
    if (shape.empty()) return body;
    return pack(shape.skip_back(), pack(shape.back(), body, dbg), dbg);
}

const Lit* World::lit_int(const Def* type, u64 i, const Def* dbg) {
    auto size = isa_sized_type(type);
    if (size->isa<Top>()) return lit(size, i, dbg);

    auto l = lit(type, i, dbg);

    if (auto a = isa_lit(size)) {
        if (err() && *a != 0 && i >= *a) err()->index_out_of_range(size, l);
    }

    return l;
}

const Def* World::global(const Def* id, const Def* init, bool is_mutable, const Def* dbg) {
    return unify<Global>(2, type_ptr(init->type()), id, init, is_mutable, dbg);
}

const Def* World::global_immutable_string(std::string_view str, const Def* dbg) {
    size_t size = str.size() + 1;

    DefArray str_array(size);
    for (size_t i = 0; i != size-1; ++i)
        str_array[i] = lit_nat(str[i], dbg);
    str_array.back() = lit_nat('\0', dbg);

    return global(tuple(str_array, dbg), false, dbg);
}

/*
 * set
 */

template<bool up>
const Def* World::ext(const Def* type, const Def* dbg) {
    if (auto arr = type->isa<Arr>()) return pack(arr->shape(), ext<up>(arr->body()), dbg);
    if (auto sigma = type->isa<Sigma>())
        return tuple(sigma, DefArray(sigma->num_ops(), [&](size_t i) { return ext<up>(sigma->op(i), dbg); }), dbg);
    return unify<TExt<up>>(0, type, dbg);
}

template<bool up>
const Def* World::bound(Defs ops, const Def* dbg) {
    auto kind = infer_kind(ops);

    // has ext<up> value?
    if (std::ranges::any_of(ops, [&](const Def* op) { return up ? bool(op->isa<Top>()) : bool(op->isa<Bot>()); }))
        return ext<up>(kind);

    // ignore: ext<!up>
    DefArray cpy(ops);
    auto [_, end] = std::ranges::copy_if(ops, cpy.begin(), [&](const Def* op) { return !isa_ext(op); });

    // sort and remove duplicates
    std::sort(cpy.begin(), end, GIDLt<const Def*>());
    end = std::unique(cpy.begin(), end);
    cpy.shrink(cpy.begin() - end);

    if (cpy.size() == 0) return ext<!up>(kind, dbg);
    if (cpy.size() == 1) return cpy[0];

    // TODO simplify mixed terms with joins and meets

    return unify<TBound<up>>(cpy.size(), kind, cpy, dbg);
}

const Def* World::et(const Def* type, Defs ops, const Def* dbg) {
    if (type->isa<Meet>()) {
        DefArray types(ops.size(), [&](size_t i) { return ops[i]->type(); });
        return unify<Et>(ops.size(), meet(types), ops, dbg);
    }

    assert(ops.size() == 1);
    return ops[0];
}

const Def* World::vel(const Def* type, const Def* value, const Def* dbg) {
    if (type->isa<Join>()) return unify<Vel>(1, type, value, dbg);
    return value;
}

const Def* World::pick(const Def* type, const Def* value, const Def* dbg) {
    return unify<Pick>(1, type, value, dbg);
}

const Def* World::test(const Def* value, const Def* probe, const Def* match, const Def* clash, const Def* dbg) {
    auto m_pi = match->type()->isa<Pi>();
    auto c_pi = clash->type()->isa<Pi>();

    if (err()) {
        // TODO proper error msg
        assert(m_pi && c_pi);
        auto a = isa_lit(m_pi->dom()->arity());
        assert(a && *a == 2);
        assert(checker_->equiv(m_pi->dom(2, 0_s), c_pi->dom()));
    }

    auto codom = join({m_pi->codom(), c_pi->codom()});
    return unify<Test>(4, pi(c_pi->dom(), codom), value, probe, match, clash, dbg);
}

/*
 * ops
 */

static const Def* tuple_of_types(const Def* t) {
    auto& world = t->world();
    if (auto sigma = t->isa<Sigma>()) return world.tuple(sigma->ops());
    if (auto arr   = t->isa<Arr  >()) return world.pack(arr->shape(), arr->body());
    return t;
}

const Def* World::op_lea(const Def* ptr, const Def* index, const Def* dbg) {
    auto [pointee, addr_space] = as<Tag::Ptr>(ptr->type())->args<2>();
    auto Ts = tuple_of_types(pointee);
    return app(app(ax_lea(), {pointee->arity(), Ts, addr_space}), {ptr, index}, dbg);
}

const Def* World::op_malloc(const Def* type, const Def* mem, const Def* dbg /*= {}*/) {
    auto size = op(Trait::size, type);
    return app(app(ax_malloc(), {type, lit_nat_0()}), {mem, size}, dbg);
}

const Def* World::op_mslot(const Def* type, const Def* mem, const Def* id, const Def* dbg /*= {}*/) {
    auto size = op(Trait::size, type);
    return app(app(ax_mslot(), {type, lit_nat_0()}), {mem, size, id}, dbg);
}

/*
 * misc
 */

std::vector<Lam*> World::copy_lams() const {
    std::vector<Lam*> result;

    for (auto def : data_.defs_) {
        if (auto lam = def->isa_nom<Lam>())
            result.emplace_back(lam);
    }

    return result;
}

#if THORIN_ENABLE_CHECKS

void World::    breakpoint(size_t number) { state_.    breakpoints.insert(number); }
void World::use_breakpoint(size_t number) { state_.use_breakpoints.insert(number); }
void World::enable_history(bool flag)     { state_.track_history = flag; }
bool World::track_history() const         { return state_.track_history; }

const Def* World::gid2def(u32 gid) {
    auto i = std::ranges::find_if(data_.defs_, [=](auto def) { return def->gid() == gid; });
    if (i == data_.defs_.end()) return nullptr;
    return *i;
}

#endif

/*
 * helpers
 */

const Def* World::dbg(Debug d) {
    auto pos2def = [&](Pos pos) { return lit_nat((u64(pos.row) << 32_u64) | (u64(pos.col))); };

    auto name = tuple_str(d.name);
    auto file = tuple_str(d.loc.file);
    auto begin = pos2def(d.loc.begin);
    auto finis = pos2def(d.loc.finis);
    auto loc = tuple({file, begin, finis});

    return tuple({name, loc, d.meta ? d.meta : bot(bot_kind()) });
}

const Def* World::infer_kind(Defs defs) const {
    for (auto def : defs) {
        if (def->type()->isa<Space>()) return def;
    }
    return kind();
}

/*
 * misc
 */

template<bool elide_empty>
void World::visit(VisitFn f) const {
    unique_queue<NomSet> noms;
    unique_stack<DefSet> defs;

    for (const auto& [name, nom] : externals()) {
        assert(nom->is_set() && "external must not be empty");
        noms.push(nom);
    }

    while (!noms.empty()) {
        auto nom = noms.pop();
        if (elide_empty && !nom->is_set()) continue;

        Scope scope(nom);
        f(scope);

        for (auto nom : scope.free_noms())
            noms.push(nom);
    }
}

/*
 * misc
 */

const Def* World::op_rev_diff(const Def* fn, const Def* dbg){
    if (auto pi = fn->type()->isa<Pi>()) {
        assert(pi->is_cn());

        auto dom = sigma(pi->dom()->ops().skip_front().skip_back());
        auto codom = sigma(pi->dom()->ops().back()->as<Pi>()->dom()->ops().skip_front());
        auto deriv_dom = tangent_type(dom,true);
        auto deriv_codom = tangent_type(codom,true);

        auto tan_dom = tangent_type(dom,false);
        auto tan_codom = tangent_type(codom,false);

        Stream s2;
        s2.fmt("dom {} => {}\n",dom,tan_dom);
        s2.fmt("codom {} => {}\n",codom,tan_codom);
        s2.fmt("dom {} =D> {}\n",dom,deriv_dom);
        s2.fmt("codom {} =D> {}\n",codom,deriv_codom);

        s2.fmt("fn {} : {}\n",fn, fn->type());

        // wrapper for fn not possible due to recursive calls

        //        auto pullback = cn_mem_ret(E,F);
        //        auto diffd = cn({
        //          type_mem(),
        //          C,
        ////          flatten(A),
        //          cn({type_mem(), D, pullback})
        //        });
        ////        auto diffd= cn_mem_flat(A,tuple({B,pullback}));
        //        // TODO: flattening at this point is useless as we handle abstract kinds here
        //        auto Xi = pi(cn_mem_ret(A, B), diffd);

        auto fn_ty = cn_mem_ret(dom,codom);
        auto pb_ty = cn_mem_ret(tan_codom,tan_dom);
        auto diff_ty = cn({type_mem(),deriv_dom,cn({type_mem(),deriv_codom,pb_ty})});

//        auto mk_pullback = app(data_.op_rev_diff_, tuple({dom, codom, deriv_dom, deriv_codom, tan_codom, tan_dom}), this->dbg("mk_pullback"));
        auto mk_pullback = app(data_.op_rev_diff_, tuple({fn_ty,diff_ty}), this->dbg("mk_pullback"));
        s2.fmt("mk pb {} : {}\n",mk_pullback,mk_pullback->type());
        auto pullback = app(mk_pullback, fn, dbg);
        s2.fmt("pb {}\n",pullback);

        return pullback;
    }

    return nullptr;
}


/*
 * misc
 */

std::string_view World::level2string(LogLevel level) {
    switch (level) {
        case LogLevel::Error:   return "E";
        case LogLevel::Warn:    return "W";
        case LogLevel::Info:    return "I";
        case LogLevel::Verbose: return "V";
        case LogLevel::Debug:   return "D";
    }
    THORIN_UNREACHABLE;
}

int World::level2color(LogLevel level) {
    switch (level) {
        case LogLevel::Error:   return 1;
        case LogLevel::Warn:    return 3;
        case LogLevel::Info:    return 2;
        case LogLevel::Verbose: return 4;
        case LogLevel::Debug:   return 4;
    }
    THORIN_UNREACHABLE;
}

#ifdef COLORIZE_LOG
std::string World::colorize(std::string_view str, int color) {
    std::string res;
    if (isatty(fileno(stdout))) {
        const char c = '0' + color;
        res = "\033[1;3";
        res += c;
        res += 'm';
        res.append(str);
        res.append("\033[0m");
    }
    return res;
}
#else
std::string World::colorize(std::string_view str, int) {
    return std::string(str);
}
#endif

void World::set(std::unique_ptr<ErrorHandler>&& err) { err_ = std::move(err); }

/*
 * instantiate templates
 */

template void Streamable<World>::write(std::string_view filename) const;
template void Streamable<World>::write() const;
template void Streamable<World>::dump() const;
template void World::visit<true >(VisitFn) const;
template void World::visit<false>(VisitFn) const;
template const Def* World::ext<true >(const Def*, const Def*);
template const Def* World::ext<false>(const Def*, const Def*);
template const Def* World::bound<true >(Defs, const Def*);
template const Def* World::bound<false>(Defs, const Def*);

}
