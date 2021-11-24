#include "thorin/pass/rw/auto_diff.h"

#include <algorithm>
#include <string>
#include <utility>

#include "thorin/analyses/scope.h"

namespace thorin {

//#define log(world,fmt,...) world.DLOG(fmt,__VA_ARGS__)
// TODO: use macros to preserve __LINE__
template<class... Args> auto log (World& world,const char* fmt, Args&&... args) {
    world.DLOG(fmt,std::forward<Args&&>(args)...);
}

void type_dump(World& world,const char* name, const Def* d) {
    world.DLOG("{} {} : {}",name,d,d->type());
}

// multidimensional addition of values
// needed for operation differentiation
// we only need a multidimensional addition
const Def* vec_add(World& world, size_t dim, const Def* a, const Def* b) {
    // adds component-wise both vectors
    Array<const Def*> ops{dim, [&](auto i) {
        return world.op(ROp::add,(nat_t)0,
                        world.extract(a,i),
                        world.extract(b,i)
        );
                          }};
    return world.tuple(ops);
}

size_t getDim(const Def* def) {
    if(auto arr=def->type()->isa<Arr>()) {
        return arr->shape()->as<Lit>()->get<uint8_t>();
    }else{
        return def->num_ops();
    }
    //                auto count=d_arg->num_ops();
    //                auto count = d_arg->type()->as<Arr>()->shape()->as<Lit>()->get<uint8_t>();
}

// Sadly, we need to "unpack" the type
const Def* lit_of_type(World& world, const Def* type, u64 lit) {

    if (auto real = isa<Tag::Real>(type))
        return world.lit_real(as_lit(real->arg()), lit);
    if (auto a = type->isa<Arr>()) {
        auto dim = a->shape()->as<Lit>()->get<uint8_t>();
        Array<const Def*> ops{dim, [&](auto i) {
            return lit_of_type(world,a->body(),lit);
        }};
        return world.tuple(ops);
    }
//    if(auto i = isa<Tag::Int>(type)) {
//        return world.lit_int(as_lit(i), lit);
//    }
    //        return world.lit_real(as_lit(real->arg()), lit);
    return world.lit_int(as_lit(as<Tag::Int>(type)), lit);
}

const Def* ONE(World& world, const Def* def) { return lit_of_type(world, def, 1); }
const Def* ZERO(World& world, const Def* def) { return lit_of_type(world, def, 0); }


namespace {

class AutoDiffer {
public:
    AutoDiffer(World& world, Def2Def  src_to_dst, Def2Def  pullbacks, const Def* A, const Def* B)
        : pullbacks_{std::move(pullbacks)}
        , world_{world}
        , src_to_dst_{std::move(src_to_dst)}
//        , idpb{}
        , A{A}
        , B{B}
    {
//        src_to_dst_=src_to_dst;
//        pullbacks_=pullbacks;

        // initializes the differentiation for a function of type A -> B
        // src_to_dst expects the parameters of the source lambda to be mapped
        //  (this property is only used later on)


        // base type of differentiation: inner
        if (auto a = A->isa<Arr>()) {
            // if the input is an array, we compute the dimension
            dim = a->shape()->as<Lit>()->get<uint8_t>();
            log(world_,"Multidimensional differentiation: {} dimensions",dim);
            // get the base type
            inner=a->body();
        }else {
            dim=1;
            log(world_,"SingleDim differentiation: {} dimensions",dim);
            inner=A;
        }
        if (auto b = B->isa<Arr>()) {
            // if the output is an array, we compute the dimension
            codim = b->shape()->as<Lit>()->get<uint8_t>();
            log(world_,"Multidimensional output differentiation: {} dimensions",codim);
        }else {
            codim=1;
            log(world_,"SingleDim output differentiation: {} dimensions",codim);
        }


        // indentity pullback for each argument for the multidimensional case
        // (one hot vector times z)
        //  <
        //      lambda z, <z,0,...,0>,
        //      lambda z, <0,z,...,0>,
        //      ...,
        //      lambda z, <0,0,...,z>,
        //  >
//        ind_idpb={
//            dim,
//            [&](auto i) {
//                Lam* ipb=world_.nom_lam(idpi, world_.dbg("id"));
//                // always expand the identity
//                ipb->set_filter(world_.lit_true());
//                Array<const Def*> ops{dim, [&](auto j) {
//                    if(i==j) // the one hot position
//                        return ipb->var(1, world_.dbg("a")); // z
//                    else // zero everywhere else
//                        return ZERO(world_,inner);
//                }};
//                const Def* opArr = world_.tuple(ops);
//                ipb->set_body(world_.app(ipb->ret_var(), {ipb->mem_var(),opArr}));
//                return ipb;
//            }
//        };
        log(world_,"Finished Construction");
    }

    const Def* reverse_diff(Lam* src); // top level function to compute the reverse differentiation of a function
    const Def* forward_diff(const Def*) { throw "not implemented"; }

    DefMap<const Def*> pullbacks_;  // <- maps a *copied* src term (a dst term) to its pullback function
private:
    const Def* j_wrap(const Def* def); // 'identity' (except for lambdas, functions, and applications) traversal annotating the pullbacks
    const Def* j_wrap_rop(ROp op, const Def* a, const Def* b); // pullback computation for predefined functions, specifically operations like +, -, *, /

    const Def* seen(const Def* src); // lookup in the map

    // chains cn[:mem, A, cn[:mem, B]] and cn[:mem, B, cn[:mem, C]] to a toplevel cn[:mem, A, cn[:mem, C]]
    const Def* chain(const Def* a, const Def* b);

    const Pi* createPbType(const Def* A, const Def* B);
    Array<const Def*> oneHot(size_t dim, size_t pos, const Def* s);

    World& world_;
    Def2Def src_to_dst_; // mapping old def to new def
//    Lam* idpb; // identity pullback;
//    Def2Def pullbacks_;  // <- maps a *copied* src term (a dst term) to its pullback function
    const Def* A;// input type
    const Def* inner;
    const Def* B; // return type
    size_t dim, codim; // dimension of input type
};

// TODO: multidim case
const Def* AutoDiffer::chain(const Def* a, const Def* b) {
    // chaining with identity is neutral
//    if (a == idpb) return b;
//    if (b == idpb) return a;

    auto at = a->type()->as<Pi>();
    auto bt = b->type()->as<Pi>();

    auto A = at->doms()[1];
    auto B = bt->doms()[1];
    auto C = bt->doms()[2]->as<Pi>()->doms()[1];
    log(world_,"   A {}",A);
    log(world_,"   B {}",B);
    log(world_,"   C {}",C);

    auto pi = world_.cn_mem_ret(A, C);
    auto toplevel = world_.nom_lam(pi, world_.dbg("chain"));

    auto middlepi = world_.cn_mem(B);
    auto middle = world_.nom_lam(middlepi, world_.dbg("chain_2"));

    toplevel->set_body(world_.app(a, {toplevel->mem_var(), toplevel->var(1), middle}));
    middle->set_body(world_.app(b, {middle->mem_var(), middle->var(1), toplevel->ret_var()}));

    toplevel->set_filter(world_.lit_true());
    middle->set_filter(world_.lit_true());

    return toplevel;
}

// pullback for a function of type A->B => pb of B result regarding A
const Pi* AutoDiffer::createPbType(const Def* A, const Def* B) {
    return world_.cn_mem_ret(B, A);
}

Array<const Def*> AutoDiffer::oneHot(size_t dim, size_t pos, const Def* s) {
    Array<const Def*> ops{dim, [&](auto i) {
        if(i==pos) // the one hot position
            return s;
        else // zero everywhere else
            // TODO: fix below (cn[mem] in extract when conditional => tuple/lam)
            if(s->type()->isa<Pi>() || isa<Tag::Mem>(s->type())) {
                return s;
            }else{
                return ZERO(world_, s->type());
            }

    }};
    return ops;
}

// top level entry point after creating the AutoDiffer object
// a mapping of source arguments to dst arguments is expected in src_to_dst
const Def* AutoDiffer::reverse_diff(Lam* src) {
    // For each param, create an appropriate pullback. It is just the (one-hot) identity function for each of those.
    //   => arguments get unit vector
    //   => pb of multidim arg is ind_idpb
    type_dump(world_,"Apply RevDiff to src",src);
    for(size_t i = 0, e = src->num_vars(); i < e; ++i) {
        auto src_param = src->var(i);
        if(src_param == src->ret_var() || src_param == src->mem_var()) {
            // skip first and last argument
            // memory and return continuation are no "real" arguments
            log(world_,"Ignore variable {} of src",i);
            continue;
        }
        auto dst = src_to_dst_[src_param];
        log(world_,"Source Param #{} {} => {} : {}",i,src_param,dst,dst->type());


        // TODO: compute A here

        size_t dim;
        if (auto a = A->isa<Arr>()) {
            dim = a->shape()->as<Lit>()->get<uint8_t>();
        }else {
            dim=1;
        }

        auto idpi = createPbType(A,A);
        log(world_,"The pullback type of the argument is {}",idpi);
        auto idpb = world_.nom_lam(idpi, world_.dbg("id"));
        idpb->set_filter(world_.lit_true());

        if(dim>1) {
            //split pullbacks for each argument
            // such that each component has one without extract
            // (needed for ROp and RCmp in the case for
            //      2d function which uses the arguments
            //      in the same order
            // )

            // TODO: unify with extract
            auto args=dst->split(dim);
            for(size_t i=0;i<dim;i++) {
                auto arg=args[i];

                auto pi = createPbType(A,arg->type());
                auto pb = world_.nom_lam(pi, world_.dbg("arg_extract_pb"));
                pb->set_filter(world_.lit_true());
                type_dump(world_,"  pb of arg_extract: ",pb);

//                auto tuple_dim = extract->tuple()->num_ops();
//                log(world_,"  extract from tuple with size {}",tuple_dim);
//                Array<const Def*> ohv{tuple_dim,
//                                      [&](auto i) { return world_.tuple(
//                                          oneHot(tuple_dim,i,pb->var(1, world_.dbg("s")))
//                                      ); }};

                pb->set_body(world_.app(
                    idpb,
                    {
                        pb->mem_var(),
                        world_.tuple(oneHot(dim,i,pb->var(1,world_.dbg("s")))),
                        pb->ret_var()
                    }
                    ));

                pullbacks_[args[i]]=pb;
            }
        }else {
        }
//        Array<const Def*> ops{dim, [&](auto i) {
//          if(dim==1) {
//              return idpb->var(1, world_.dbg("s"));
//          }else{
//              return world_.extract_unsafe(idpb->var(1, world_.dbg("s")), i);
//          }
//        }};
//        log(world_,"Ops: {}",ops);
//        const Def* opArr = world_.tuple(ops);
        // shorten to variable input => id
        idpb->set_body(world_.app(idpb->ret_var(),
            {idpb->mem_var(),idpb->var(1,world_.dbg("s"))}));

        pullbacks_[dst] = idpb;

        type_dump(world_,"dst ",dst);
        type_dump(world_,"Pullback of dst ",pullbacks_[dst]);
    }
    log(world_,"Initialization finished, start jwrapping");
    // translate the body => get correct applications of variables using pullbacks
    auto dst = j_wrap(src->body());
    return dst;
}

// We implement AD in a similar way as described by Brunel et al., 2020
//  <x², λa. x'(a * 2*x)>
//       ^^^^^^^^^- pullback. The intuition is as follows:
//                            Each value x has a pullback pb_x.
//                            pb_x receives a value that was differentiated with respect to x.
//                  Thus, the "initial" pullback for parameters must be the identity function.
// Here is a very brief example of what should happen in `j_wrap` and `j_wrap_rop`:
//
//      SOURCE             |  PRIMAL VERSION OF SOURCE
//   ----------------------+-----------------------------------------------------------------------
//     // x is parameter   | // <x,x'> is parameter. x' should be something like λz.z
//    let y = 3 * x * x;   | let <y,y'> = <3 * x * x, λz. x'(z * (6 * x))>;
//    y * x                | <y * x, λz. y'(z * x) + x'(z * y)>
//
// Instead of explicitly putting everything into a pair, we just use the pullbacks freely
//  Each `x` gets transformed to a `<x, λδz. δz * (δz / δx)>`
//
// return src_to_dst[src] => dst
const Def* AutoDiffer::j_wrap(const Def* def) {
    //    if(isa<Tag::Mem>(def->type())) {
    //        return def; // and pb is not relevant for memory
    //    }
    type_dump(world_,"J_wrap of ",def);
    log(world_,"  Node: {}",def->node_name());

    if (auto dst = seen(def)) {
        // we have converted def and already have a pullback
        type_dump(world_,"already seen",def);
        return dst;
    }

    if (auto var = def->isa<Var>()) {
        // variable like whole lambda var should not appear here
        // variables should always be differentiated with their function/lambda context
        type_dump(world_,"Error: variable out of scope",var);
        THORIN_UNREACHABLE;
    }
    if (auto axiom = def->isa<Axiom>()) {
        // an axiom without application has no meaning as a standalone term
        type_dump(world_,"Error: axiom",axiom);

        // for nested derivs, handled in app
//        if(axiom->tag()==Tag::RevDiff) {
//            type_dump(world_,"Error: Rethrow rev_diff axiom",axiom);
//            return def;
//        }
        log(world_,"  axiom has tag {}",axiom->tag());

        THORIN_UNREACHABLE;
    }
    if (auto lam = def->isa_nom<Lam>()) {
        // lambda => a function (for instance then and else for conditions)
        // TODO: need closure conversion?
        type_dump(world_,"Lam",lam);
        auto old_pi = lam->type()->as<Pi>();

        // TODO: distinguish between returning and non-returning
        //      => necessary? (are there returning lambdas in this position?)
        log(world_,"  lam args {}",old_pi->num_doms());
        if(old_pi->num_doms()==1){//only mem argument
            // keep everything as it is
            // and differentiate body
            // TODO: merge with else case
            log(world_,"  non-returning mem lambda");
            auto dst = world_.nom_lam(old_pi, world_.dbg(lam->name()));
            type_dump(world_,"  => ",dst);
            src_to_dst_[lam->var()] = dst->var();
            type_dump(world_,"  dst var (no pb needed): ",dst->var());
            dst->set_filter(lam->filter());

            auto bdy = j_wrap(lam->body());
            dst->set_body(bdy);
            src_to_dst_[lam] = dst;
            // the pullback of a lambda without call or arguments is the identity
//            pullbacks_[dst] = idpb; // TODO: correct? needed?

            // never executed but needed for tuple pb
            auto zeropi = createPbType(A,lam->type());
            auto zeropb = world_.nom_lam(zeropi, world_.dbg("zero_pb"));
            type_dump(world_,"  non ret pb (zero)",zeropb);
            zeropb->set_filter(world_.lit_true());
            auto zero = ZERO(world_, A);
            zeropb->set_body(world_.app(zeropb->ret_var(), {zeropb->mem_var(), zero}));
            pullbacks_[dst] =zeropb;

            return dst;
        }

        // take a pullback additionally to the argument
        auto pi = world_.cn({world_.type_mem(), old_pi->doms()[1], createPbType(A,old_pi->doms()[1])});
//        auto pi = world_.cn_mem_ret(old_pi->doms()[1], A);
        auto dst = world_.nom_lam(pi, world_.dbg(lam->name()));
        type_dump(world_,"  => ",dst);
        src_to_dst_[lam->var()] = dst->var();
        type_dump(world_,"  dst var: ",dst->var());
        pullbacks_[dst->var()] = dst->var(dst->num_vars() - 1); // pullback (for var) is the last argument
        type_dump(world_,"  dst var pb: ",pullbacks_[dst->var()]);
        dst->set_filter(lam->filter());

        // same as above: jwrap body
        auto bdy = j_wrap(lam->body());
        dst->set_body(bdy);
        src_to_dst_[lam] = dst;
        pullbacks_[dst] = pullbacks_[bdy]; // TODO: correct? needed?
        return dst;
    }
    if (auto app = def->isa<App>()) {
        // the most complicated case: an application
        // we basically distinguish four cases:
        // * operation
        // * comparison
        // * returning function call
        // * not-returning function call

        type_dump(world_,"App",app);
        auto callee = app->callee();
        auto arg = app->arg();
        type_dump(world_,"  callee",callee);
        type_dump(world_,"  arg",arg);

        // Handle binary operations
        if (auto inner = callee->isa<App>()) {
            log(world_,"  app of app");
            // Take care of binary operations


            if (auto axiom = inner->callee()->isa<Axiom>()) {
                log(world_,"  app of axiom * args");

                if (axiom->tag() == Tag::RevDiff) {
                    type_dump(world_,"  wrap op rev_diff of ",arg);
                    auto dst_callee = world_.op_rev_diff(arg);
                    type_dump(world_,"  result  ",dst_callee);
                    return dst_callee;
                }

                if (axiom->tag() == Tag::ROp) {
                    type_dump(world_,"  ROp",axiom);
                    auto ab = j_wrap(arg);
                    type_dump(world_,"  args jwrap",ab);
                    auto [a, b] = ab->split<2>();
//                    if(!pullbacks_.count(a) || !pullbacks_.count(b)){
//                        // necessary for non-extracted components of main function argument
//                        // => the array function argument has a pullback (tuple)
//                        //    but the components do not (not registered)
//                        // TODO: maybe move up to reverse_diff?
//                        auto [pa,pb]=pullbacks_[ab]->split<2>();
//                        type_dump(world_,"  manually split pullbacks",pullbacks_[ab]);
//                        pullbacks_[a]=pa;
//                        pullbacks_[b]=pb;
//                    }
                    auto dst = j_wrap_rop(ROp(axiom->flags()), a, b);
                    src_to_dst_[app] = dst;
                    type_dump(world_,"  result of app",dst);
                    return dst;
                }

                if (axiom->tag() == Tag::RCmp) {
                    type_dump(world_,"  RCmp",axiom);
//                    auto [a, b] = j_wrap(arg)->split<2>();
//                    type_dump(world_,"  arg jwrap a",a);
//                    type_dump(world_,"  arg jwrap b",b);
                    auto ab = j_wrap(arg);
                    type_dump(world_,"  args jwrap",ab);
                    auto [a, b] = ab->split<2>();
//                    if(!pullbacks_.count(a) || !pullbacks_.count(b)){
//                        // necessary for non-extracted components of main function argument
//                        // => the array function argument has a pullback (tuple)
//                        //    but the components do not (not registered)
//                        // TODO: maybe move up to reverse_diff?
//                        auto [pa,pb]=pullbacks_[ab]->split<2>();
//                        type_dump(world_,"  manually split pullbacks",pullbacks_[ab]);
//                        pullbacks_[a]=pa;
//                        pullbacks_[b]=pb;
//                    }
                    auto dst = world_.op(RCmp(axiom->flags()), nat_t(0), a, b);
                    src_to_dst_[app] = dst;
                    type_dump(world_,"  result of app",dst);
                    // TODO: tuple or app
//                    return world_.tuple({inner, dst});
                    return dst;
                }
            }
        }



        if (callee->type()->as<Pi>()->is_returning()) {
            log(world_,"  FYI returning callee");
            // for function calls
            // TODO: error with inhomogeneous calls and composition

            auto dst_callee = world_.op_rev_diff(callee);
            type_dump(world_,"  Used RevDiff Op on callee",dst_callee);
            log(world_,"  this call will invoke AutoDiff rewrite");
            auto d_arg = j_wrap(arg);
            type_dump(world_,"  wrapped args: ",d_arg);


            auto [m,arg,ret_arg] = d_arg->split<3>();
//            auto ret = ret_arg;
            type_dump(world_,"  split wrapped args into: mem: ",m);
            type_dump(world_,"  split wrapped args into: arg: ",arg);
            type_dump(world_,"  split wrapped args into: ret: ",ret_arg);

            // apply ret to expected mem, res, but custom continuation
//            auto dst = world_.app(dst_callee, {m,arg,ret});


//            auto pbT = ret->type()->as<Pi>();
            auto pbT = dst_callee->type()->as<Pi>()->doms().back()->as<Pi>();
            auto chained = world_.nom_lam(pbT, world_.dbg("φchain"));
            type_dump(world_,"  chained pb will be (app pb) ",chained);

//            type_dump(world_,"  arg pb",pullbacks_[d_arg]);
//            log(world_,"  arg pb node: {}",pullbacks_[d_arg]->node_name());
//            type_dump(world_,"  ret var pb",chained->ret_var());
//            log(world_,"  ret var pb node: {}",chained->ret_var()->node_name());

            auto arg_pb = pullbacks_[d_arg]; // Lam
            auto ret_pb = chained->ret_var(); // extract
            type_dump(world_,"  arg pb",arg_pb);
            type_dump(world_,"  ret var pb",ret_pb);
            auto chain_pb = chain(ret_pb,arg_pb);
            type_dump(world_,"  chain pb",chain_pb);


            chained->set_body( world_.app(
                ret_arg,
                {
                    chained->mem_var(),
                    chained->var(1),
                    chain_pb
//                    chain(arg_pb,ret_pb)
                }
//                ret, // d_arg->ret_var()
////                    chained->vars()
//                {
//                    chained->mem_var(),
////                    chained->var((size_t)0),
//                    chained->var(1),
////                    chained->var(2)
////                    chained->ret_var()
////                    chain(arg_pb,ret_pb)
//                    chain(ret_pb,arg_pb)
//                }
                ));
            chained->set_filter(world_.lit_true());
            type_dump(world_,"  build chained (app pb) ",chained);

            auto dst = world_.app(dst_callee, {m,arg,chained});

//            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), one), end}));
//            auto adiff = middle->var(1);
//            auto bdiff = end->var(1);
//
//            auto sum = vec_add(world_, dim, adiff, bdiff);
//            end->set_body(world_.app(pb->ret_var(), { end->mem_var(), sum}));


//            auto dst = world_.app(dst_callee, d_arg);
            type_dump(world_,"  application with jwrapped args",dst);

            pullbacks_[dst] = pullbacks_[d_arg]; // TODO: where is this pb used?
            type_dump(world_,"  pullback of dst (call app): ",pullbacks_[dst]);
            // TODO: why no registration in src_to_dst
            // TODO: overwrite pullback after reverse_diff => know diff of functions

            return dst;

            // TODO: do something special
//            THORIN_UNREACHABLE;
        }else {
            log(world_,"  FYI non-returning callee");
            // TODO: move out of if
            auto d_callee= j_wrap(callee);
            type_dump(world_,"  wrapped callee: ",d_callee);
            auto d_arg = j_wrap(arg);
            type_dump(world_,"  wrapped args: ",d_arg);
            log(world_,"  arg in pb: {}",pullbacks_.count(d_arg));
            if(pullbacks_.count(d_arg))
                type_dump(world_,"  arg pb: ",pullbacks_[d_arg]);
            log(world_,"  type: {}",d_arg->node_name());
            const Def* ad_args;
//            Array<const Def*> ad_args;
            // TODO: one should rather look at the type if it is a tuple type

            // TODO: what is correct here
            if(d_arg->isa<Tuple>()) {
                log(world_,"  tuple argument");
//                auto count=d_arg->num_ops();
                auto count=getDim(d_arg);
//                auto count = d_arg->type()->as<Arr>()->shape()->as<Lit>()->get<uint8_t>();
                log(world_,"  count: {}",count);
                ad_args = world_.tuple(
                    Array<const Def*>(
                    count+1,
                    [&](auto i) {if (i<count) {return world_.extract(d_arg, (u64)i, world_.dbg("ad_arg"));} else {return pullbacks_[d_arg];}}
                ));
            }else {
                // var (lambda completely with all arguments) and other (non tuple)
                log(world_,"  non tuple argument");
                // extract like Mem@
//                ad_args={d_arg,pullbacks_[d_arg]};
//                ad_args={d_arg};
                ad_args = d_arg;
            }
//            auto dst = world_.app(j_wrap(callee), world_.tuple({d_arg, pullbacks_[d_arg]}));
            auto dst = world_.app(d_callee, ad_args);
            src_to_dst_[app] = dst;
            return dst;
        }
    }

    if (auto tuple = def->isa<Tuple>()) {
        // the pullback of a tuple is tuple of pullbacks for each component
        // we need to distinguish [mem, r32] from <<2::nat,r32>>
        // a tuple with memory as argument is used in applications but we only want the pullback of the "real" arguments
        type_dump(world_,"tuple",tuple);
//        auto tuple_dim = tuple->num_ops();
        auto tuple_dim=getDim(tuple);
//        auto tuple_dim = tuple->type()->as<Arr>()->shape()->as<Lit>()->get<uint8_t>();
        log(world_,"  num of ops: {}",tuple_dim);
        // jwrap each component
        Array<const Def*> ops{tuple_dim, [&](auto i) { return j_wrap(tuple->op(i)); }};
        // reconstruct the tuple term
        auto dst = world_.tuple(ops);
        type_dump(world_,"  jwrapped tuple:",dst);
        src_to_dst_[tuple] = dst;

        if(isa<Tag::Mem>(tuple->op(0)->type())) {
            log(world_,"  mem pb tuple");
            pullbacks_[dst] = pullbacks_[ops[1]];
            return dst;
        }


        // TODO: this seems excessively complicated

        auto pi = createPbType(A,tuple->type());
        auto pb = world_.nom_lam(pi, world_.dbg("tuple_pb"));
        log(world_,"  complete tuple pb type: {}",pi);
        pb->set_filter(world_.lit_true());

        type_dump(world_,"  A:",A);
//        log(world_,"  A node name: {}",A->node_name());
//        auto pbT = A->as<Pi>();
        auto pbT = pi->as<Pi>()->doms().back()->as<Pi>();
        log(world_,"  intermediate tuple pb type: {}",pbT);
        log(world_,"  should be cn_mem of {}",A);
        auto cpb = pb;
        auto sum=ZERO(world_,A);
        Lam* nextpb;

        log(world_,"  sum up tuple derivatives for ops {}",ops);

        for (size_t i = 0; i < tuple_dim; ++i) {
            log(world_,"  op {} = {} : {}",i,ops[i],ops[i]->type());
            log(world_,"  has pb? {}",pullbacks_.count(ops[i]));
            if(!pullbacks_.count(ops[i])) {
                ops[i]=j_wrap(tuple->op(i));
            }
            type_dump(world_,"  has pb",pullbacks_[ops[i]]);
            nextpb = world_.nom_lam(pbT, world_.dbg("φtuple_next"));
            nextpb->set_filter(world_.lit_true());
            cpb->set_body(
                world_.app(pullbacks_[ops[i]],
                    {cpb->mem_var(),
                    world_.extract_unsafe(pb->var(1, world_.dbg("s")), i),
                    nextpb
                    }));
            cpb=nextpb;
            //all nextpb args are result
            sum=vec_add(world_,dim,sum,nextpb->var(1));
        }
        nextpb->set_body( world_.app( pb->ret_var(), {nextpb->mem_var(),sum} ));





//        auto pi = createPbType(A,tuple->type());
//        auto pb = world_.nom_lam(pi, world_.dbg("tuple_pb"));
//        pb->set_filter(world_.lit_true());


//        Array<const Def*> pbops{dim, [&](auto i) {
//              return world_.app(
//                  pullbacks_[ops[i]],
//                  world_.extract_unsafe(pb->var(1, world_.dbg("s")), i)
//              );
//        }};
//        pb->set_body(world_.app(pb->ret_var(), {pb->mem_var(),world_.tuple(pbops)}));

        // TODO: multiple arguments
        // TODO: double diff? [mem, r32,
        //      cn[mem, r32, cn[mem, r32, cn[mem, r32]]]]

        log(world_,"  tuple pbs {}",pb);
        // ret (mem, res) is an app with tuple as arg
        // we want
        // ret' (mem, res, pb) => pb of arg/res but not again a tuple (ignore mem)
        pullbacks_[dst]=pb;
        type_dump(world_,"  pullback for tuple",pullbacks_[dst]);
        return dst;
    }

    if (auto pack = def->isa<Pack>()) {
        type_dump(world_,"Pack",pack);
        auto dst = world_.pack(pack->type()->arity(), j_wrap(pack->body()));
        src_to_dst_[pack] = dst;
        type_dump(world_,"  jwrapped pack",dst);
//        pullbacks_[dst] = idpb; // TODO: check
        log(world_,"  we need no pb for pack, right?");
//        type_dump(world_,"  pullback of pack (idpb)",pullbacks_[dst]);
        return dst;
    }

    if (auto extract = def->isa<Extract>()) {
        // when extracting a component, the pullback is extracted from the tuple pullback of the tuple argument
        type_dump(world_,"Extract",extract);
        auto jtup = j_wrap(extract->tuple());
        type_dump(world_,"  jwrapped tuple of extract",jtup);
        type_dump(world_,"  extract idx",extract->index());
        auto jeidx= j_wrap(extract->index());
        type_dump(world_,"  extract wrapped idx",jeidx);
        auto dst = world_.extract_unsafe(jtup, jeidx);
        type_dump(world_,"  jwrapped extract",dst);
        src_to_dst_[extract] = dst;
        // do not extract diff
        // but tuple => tuple of diffs
        // no lambda

//        log(world_,"  tuple first type: {}",jtup->type()->op(0));

        if(isa<Tag::Mem>(jtup->type()->op(0))) {
            log(world_,"  extract mem pb tuple ");
            pullbacks_[dst] = pullbacks_[jtup];
            type_dump(world_,"  pullback of extract",pullbacks_[dst]);
            return dst;
        }


        auto pi = createPbType(A,extract->type());
        auto pb = world_.nom_lam(pi, world_.dbg("extract_pb"));
        pb->set_filter(world_.lit_true());
        type_dump(world_,"  pb of extract: ",pb);

//        auto tuple_dim = extract->tuple()->num_ops();
        auto tuple_dim=getDim(jtup);
//        auto tuple_dim = jtup->type()->as<Arr>()->shape()->as<Lit>()->get<uint8_t>();
        type_dump(world_,"  extract from tuple",extract->tuple());
        log(world_,"  extract from tuple with size {}",tuple_dim);
        Array<const Def*> ohv{tuple_dim,
                              [&](auto i) { return world_.tuple(
                                  oneHot(tuple_dim,i,pb->var(1, world_.dbg("s")))
                              ); }};

        pb->set_body(world_.app(
            pullbacks_[jtup],
            {
                pb->mem_var(),
                world_.extract_unsafe(world_.tuple(ohv), extract->index()),
                pb->ret_var()
            }
        ));
        pullbacks_[dst] = pb;
        type_dump(world_,"  pullback of extract",pullbacks_[dst]);
        return dst;
    }

    if (auto insert = def->isa<Insert>()) {
        // the pullback for an insertion is an insertion of a pullback into the tuple pullback
        type_dump(world_,"Insert",insert);
        auto dst = world_.insert(j_wrap(insert->tuple()), insert->index(), j_wrap(insert->value()));
        src_to_dst_[insert] = dst;
        type_dump(world_,"  jwrapped insert",dst);
        // TODO: correct pullback
//        pullbacks_[dst] = idpb; // TODO: check
//        type_dump(world_,"  pullback of insert (idpb)",pullbacks_[dst]);
        log(world_,"  TODO: pullback of insert is currently missing");
        return dst;
    }

    if (auto lit = def->isa<Lit>()) {
        // a literal (number) has a zero pullback
        type_dump(world_,"Literal",lit);
        // The derivative of a literal is ZERO
        // TODO: currently only for r32 literals
//        auto zeropi = world_.cn_mem_ret(lit->type(), A);
        auto zeropi = world_.cn_mem_ret(inner, A);
        auto zeropb = world_.nom_lam(zeropi, world_.dbg("zero_pb"));
        type_dump(world_,"  lit pb (zero)",zeropb);
        zeropb->set_filter(world_.lit_true());
        auto zero = ZERO(world_, A);// or use dim directly
        zeropb->set_body(world_.app(zeropb->ret_var(), {zeropb->mem_var(), zero}));
        // TODO: no src_to_dst mapping?
        //   trivial construct => not necessary
        pullbacks_[lit] = zeropb;
        return lit;
    }

    type_dump(world_,"unhandeled def",def);
    log(world_,"  node {}",def->node_name());
    THORIN_UNREACHABLE;
}


// translates operation calls and creates the pullbacks
const Def* AutoDiffer::j_wrap_rop(ROp op, const Def* a, const Def* b) {
    // build up pullback type for this expression
    auto o_type = a->type(); // type of the operation
    auto pbpi = createPbType(A,o_type);
    auto pbT = pullbacks_[a]->type()->as<Pi>()->doms().back()->as<Pi>(); // TODO: create using A
    auto pb = world_.nom_lam(pbpi, world_.dbg("φ"));

    // shortened pullback type => takes pullback result (A) And continues
    auto middle = world_.nom_lam(pbT, world_.dbg("φmiddle"));
    auto end = world_.nom_lam(pbT, world_.dbg("φend"));

    // always expand operation pullbacks
    pb->set_filter(world_.lit_true());
    middle->set_filter(world_.lit_true());
    end->set_filter(world_.lit_true());

    // constant for calculations
    auto one = ONE(world_, o_type);

    // Grab argument pullbacks
    assert(pullbacks_.count(a) && "Pullbacks for ROp arguments should already be created");
    assert(pullbacks_.count(b) && "Pullbacks for ROp arguments should already be created");
    // pullbacks of the arguments
    auto apb = pullbacks_[a];
    auto bpb = pullbacks_[b];
    // compute the pullback for each operation
    // general procedure:
    //  pb  computes a*(...) continues in mid
    //  mid computed b*(...) continues in end
    //  end computes the addition of the result of pb (arg of mid) and the result of mid (arg of end),
    //    adds them together using vector addition, and returns the result using the
    //    pullback return function from pb
    //  <f(x); λ z. Σ xᵢ*( ∂ᵢf(x) ⋅ z )>
    switch (op) {
        // ∇(a + b) = λz.∂a(z * (1 + 0)) + ∂b(z * (0 + 1))
        case ROp::add: {
            auto dst = world_.op(ROp::add, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "+"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), one), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), one), end}));
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto sum = vec_add(world_, dim, adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { end->mem_var(), sum}));
            pullbacks_[dst] = pb;

            return dst;
        }
        // ∇(a - b) = λz.∂a(z * (0 + 1)) - ∂b(z * (0 + 1))
        case ROp::sub: {
            // φ-(z,ret):
            //  pba(z*1,φm-)
            // φm-(x):
            //  pbb(z*-1,φe-)
            // φe-(y):
            //  ret(x+y)
            //
            // a*(z)+b*(-z)
            auto dst = world_.op(ROp::sub, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "-"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), one), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), world_.op_rminus((nat_t)0, one)), end}));
            // all args 1..n as tuple => vector for addition
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto sum = vec_add(world_, dim, adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { end->mem_var(), sum}));
            pullbacks_[dst] = pb;

            return dst;
        }
            // ∇(a * b) = λz.∂a(z * (1 * b + a * 0)) + ∂b(z * (0 * b + a * 1))
            //          potential opt: if ∂a = ∂b, do: ∂a(z * (a + b))
            //             do this in the future. We need to make sure the pb is linear.
            //             This should be doable without additional tracking if we change
            //             their types from `R -> R` to `R -> ⊥`
        case ROp::mul: {
            // φ*(z,ret):
            //  pba(z*b,φm*)
            // φm*(x):
            //  pbb(z*a,φe*)
            // φe*(y):
            //  ret(x+y)
            //
            // a*(zb)+b*(za)
            auto dst = world_.op(ROp::mul, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "*"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), b), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), a), end}));
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto sum = vec_add(world_, dim, adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { end->mem_var(), sum}));
            pullbacks_[dst] = pb;
            return dst;
        }
            // ∇(a / b) = λz. (g* (z * h) - h* (z * g))/h²
        case ROp::div: {
            //    a*(1/b * z)          => a*(z/b)
            //  + b*(a * -b^(-2) * z)  => b*(z*a/(b*b))
            auto dst = world_.op(ROp::div, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "/"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::div, (nat_t)0, pb->var(1), b), middle}));
            auto za=world_.op(ROp::mul, (nat_t)0, pb->var(1), a);
            auto bsq=world_.op(ROp::mul, (nat_t)0, b, b);
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op_rminus((nat_t)0, world_.op(ROp::div, (nat_t)0, za, bsq)), end}));
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto sum = vec_add(world_, dim, adiff, bdiff);

            end->set_body(world_.app(pb->ret_var(), { end->mem_var(), sum}));
            pullbacks_[dst] = pb;
            return dst;
        }
        default:
            // only +, -, *, / are implemented as basic operations
            THORIN_UNREACHABLE;
    }
}

// seen is a simple lookup in the src_to_dst mapping
const Def* AutoDiffer::seen(const Def* def) { return src_to_dst_.contains(def) ? src_to_dst_[def] : nullptr; }

} // namespace

// rewrites applications of the form 'rev_diff function' into the differentiation of f
const Def* AutoDiff::rewrite(const Def* def) {
    if (auto app = def->isa<App>()) {
        if (auto type_app = app->callee()->isa<App>()) {
            if (auto axiom = type_app->callee()->isa<Axiom>(); axiom && axiom->tag() == Tag::RevDiff) {
                // rev_diff(f)
                // in thorin :rev_diff ‹2∷nat; r32› f
                //           --------- app ----------
                //           ------ type_app ------ arg
                //           (axiom    arg2       ) arg

                auto src_lam = app->arg(0)->as_nom<Lam>();
                // function to differentiate
                // this should be something like `cn[:mem, r32, cn[:mem, r32]]`
                auto& world = src_lam->world();

                // We get for `A -> B` the type `A -> (B * (B -> A))`.
                //  i.e. cn[:mem, A, [:mem, B]] ---> cn[:mem, A, cn[:mem, B, cn[:mem, B, cn[:mem, A]]]]
                //  take input, return result and return a function (pullback) taking z and returning the derivative
                auto dst_pi = app->type()->as<Pi>(); // multi dim as array
                auto dst_lam = world.nom_lam(dst_pi, world.dbg("top_level_rev_diff_" + src_lam->name()));
                dst_lam->set_filter(src_lam->filter()); // copy the unfold filter
                auto A = dst_pi->dom(1); // input variable(s) => possible a pi type (array)
                auto B = src_lam->ret_var()->type()->as<Pi>()->dom(1); // the output (for now a scalar)


                log(world,"AD of function from {} to {}",A,B);
                type_dump(world,"Transform:",src_lam);
                type_dump(world,"Result:",dst_lam);

                // The actual AD, i.e. construct "sq_cpy"
//                Def2Def src_to_dst;
                // src_to_dst maps old definitions to new ones
                // here we map the arguments of the lambda
                for (size_t i = 0, e = src_lam->num_vars(); i < e; ++i) {
                    auto src_param = src_lam->var(i);
                    auto dst_param = dst_lam->var(i, world.dbg(src_param->name()));
                    // the return continuation changes => special case
                    src_to_dst[src_param] = i == e - 1 ? dst_lam->ret_var() : dst_param;
                }
                auto differ = AutoDiffer{world, src_to_dst, pullbacks, A, B};
                dst_lam->set_body(differ.reverse_diff(src_lam));

                pullbacks=differ.pullbacks_;

                return dst_lam;
            }
        }
    }

    return def;
}

}
