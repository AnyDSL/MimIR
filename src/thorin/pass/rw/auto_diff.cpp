#include "thorin/pass/rw/auto_diff.h"

#include <algorithm>
#include <string>

#include "thorin/analyses/scope.h"

namespace thorin {

#define dlog(world,...) world.DLOG(__VA_ARGS__)
#define type_dump(world,name,d) world.DLOG("{} {} : {}",name,d,d->type())


size_t getDim(const Def* def) {
    // TODO: test def, idef, tuple
    if(auto arr=def->isa<Arr>()) {
        return arr->shape()->as<Lit>()->get<uint8_t>();
    }else if(auto arr=def->type()->isa<Arr>()) {
        return getDim(def->type());
    }else{
        dlog(def->world(),"  def dim {} : {}, dim {}",def,def->type(),def->num_projs());
        return def->num_projs();
        // ptr -> 1
        // tuple -> size
    }
}


// multidimensional addition of values
// needed for operation differentiation
// we only need a multidimensional addition
std::pair<const Def*,const Def*> vec_add(World& world, const Def* mem, const Def* a, const Def* b) {
    dlog(world,"add {}:{} + {}:{}",a,a->type(),b,b->type());

    if (auto aptr = isa<Tag::Ptr>(a->type())) {
        auto [ty,addr_space] = aptr->arg()->projs<2>();

        auto [mem2,a_v] = world.op_load(mem,a)->projs<2>();
        auto [mem3,b_v] = world.op_load(mem2,b)->projs<2>();

        auto [mem4, s_v] = vec_add(world,mem3,a_v,b_v);

        auto [mem5, sum_ptr]=world.op_slot(ty,mem4,world.dbg("add_slot"))->projs<2>();
        auto mem6 = world.op_store(mem3,sum_ptr,s_v);
        return {mem6, sum_ptr};
    }

    // TODO: correct handling of mixed tuple, def array
    // TODO: handling of idef
    // lift only for idef (in the future)
    //   and non-mixed tuple (and array with hack)

//    if(auto arr = a->type()->isa<Arr>()) {
    if(auto arr = a->type()->isa<Arr>(); arr && !(arr->shape()->isa<Lit>())) {
//    if(auto arr = a->type()->isa<Arr>();false) {
        dlog(world,"  Array add");
        auto shape = arr->shape();
        dlog(world,"  Array shape {}", shape);
        dlog(world,"  Array {}", arr);

        auto body_type = arr->body();
        while(auto barr = body_type->isa<Arr>()) {
            body_type = barr->body();
        }

        // tangents are only reals
        nat_t bit_width = as_lit(as<Tag::Real>(body_type)->arg());

        type_dump(world,"  Array Body", body_type);
        dlog(world,"  Bit width {}", bit_width);
        #define w world
        auto lifted=w.app(w.app(w.app(w.ax_lift(),
                        // rs => sigma(r:nat, s:arr with size r of nat)
                        // r = how many dimensions in the array
                        // s = dimensions
            {w.lit_nat(1), shape}), // w.tuple({shape})

                  // is_os = [ni, Is, no, Os, f]
                  // ni:nat how many base input dims
                  // Is: <n_i;*> type array os size ni => base input types
                  // no:nat how many base out dims
                  // Os: <n_o;*> type array os size no => base output types
                  // f: arr of size ni of types Is
                  //    to arr of size no of types Os
            {w.lit_nat(2),w.tuple({body_type,body_type}),
                  w.lit_nat(1), body_type,
                  w.fn(ROp::add, (nat_t)0, bit_width)
                  }),
            world.tuple({a,b}));
        type_dump(world,"  lifted",lifted);
        return {mem, lifted};
    }

    auto dim = getDim(a);

    if(dim==1){
        return {mem, world.op(ROp::add,(nat_t)0,a,b)};
    }

    Array<const Def*> ops{dim};
    for (size_t i = 0; i < ops.size(); ++i) {
        // adds component-wise both vectors
        auto [nmem, op]=vec_add( world,mem, world.extract(a,i), world.extract(b,i) );
        mem=nmem;
        ops[i]=op;
    }
    return {mem, world.tuple(ops)};
}

std::pair<const Def*,const Def*> lit_of_type(World& world, const Def* mem, const Def* type, r64 lit, const Def* dummy) {
    // TODO: a monad would be easier for memory
    dlog(world,"create literal of type {}",type);

    // TODO: not for idef array
    if (auto ptr = isa<Tag::Ptr>(type)) {
        auto [ty,addr_space] = ptr->arg()->projs<2>();

        if(ty->isa<Arr>()) {
            auto [mem2,ptr_arr]=world.op_alloc(ty,mem)->projs<2>();
            type_dump(world,"ptr arr",ptr_arr);
            return {mem2,ptr_arr};
        }

        auto [mem2, lit_ptr]=world.op_slot(ty,mem,world.dbg("lit_slot"))->projs<2>();
        auto [mem3, lit_res] = lit_of_type(world,mem2,ty,lit,dummy);
        auto mem4 = world.op_store(mem3,lit_ptr,lit_res);

        return {mem4,lit_ptr};
    }
    const Def* litdef;
    if (auto real = isa<Tag::Real>(type))
        litdef= world.lit_real(as_lit(real->arg()), lit);
    else if (auto a = type->isa<Arr>()) {
        auto dim = a->shape()->as<Lit>()->get<uint8_t>();
        dlog(world,"create array literal of dim {}",dim);
        Array<const Def*> ops{dim};
        for (size_t i = 0; i < dim; ++i) {
            auto [nmem, op]=lit_of_type(world,mem,a->body(),lit,dummy);
            mem=nmem;
            ops[i]=op;
        }
        litdef= world.tuple(ops);
    }else if(auto sig = type->isa<Sigma>()) {
        std::vector<const Def*> zops;
        dlog(world,"create tuple (Sigma) literal of dim {}",sig->num_ops());
        for (auto op : sig->ops()) {
            auto [nmem, zop]=lit_of_type(world,mem,op,lit,dummy);
            mem=nmem;
            zops.push_back(zop);
        }
        litdef= world.tuple(zops);
    }
    else litdef= dummy;

    return {mem,litdef};
}

std::pair<const Def*,const Def*> ONE(World& world, const Def* mem, const Def* def, const Def* dummy) { return lit_of_type(world, mem, def, 1, dummy); }
std::pair<const Def*,const Def*> ZERO(World& world, const Def* mem, const Def* def, const Def* dummy) { return lit_of_type(world, mem, def, 0, dummy); }
std::pair<const Def*,const Def*> ZERO(World& world, const Def* mem, const Def* def) { return ZERO(world,mem, def, nullptr);}
std::pair<const Def*,const Def*> ONE(World& world, const Def* mem, const Def* def) { return ONE(world,mem, def, nullptr);}


std::pair<const Def*,const Def*> oneHot(World& world_, const Def* mem,u64 idx, const Def* shape, const Def* s) {
    auto [rmem, v] = ZERO(world_,mem,shape,s);
    return {rmem,world_.insert_unsafe(v,idx,s)};
}

std::pair<const Def*,const Def*> oneHot(World& world_, const Def* mem,const Def* idx, const Def* shape, const Def* s) {
    // TODO: extend for different shapes => indef array
    // can one do better for a def array shape? => insert

    // TODO: insert for array; alloc for idef

    type_dump(world_,"OH Shape: ",shape);
    type_dump(world_,"OH Idx: ",idx);

    if(shape->isa<Pi>()) {
        dlog(world_,"Pi shape");
    }
    if(shape->isa<Arr>()) {
        dlog(world_, "Arr shape");
    }

    if(auto lit = isa_lit(idx)) {
        type_dump(world_, "lit oh of type ", shape);
        return oneHot(world_,mem,*lit,shape,s);
    }else {
        dlog(world_, "non-lit oh");
        auto dim = getDim(shape);
        dlog(world_,"dim: {}",dim);

        Array<const Def*> ohv{dim};
        for (size_t i = 0; i < dim; ++i) {
            auto [nmem, oh]=oneHot(world_,mem,i,shape,s);
            mem=nmem;
            ohv[i]=oh;
        }
        dlog(world_, "creates ohv: ");
        auto t = world_.tuple(ohv);
        type_dump(world_, "as tuple: ",t);
        return {mem,world_.extract_unsafe(world_.tuple(ohv),idx)};
    }
}





namespace {

class AutoDiffer {
public:
    AutoDiffer(World& world, const Def2Def& src_to_dst, const Def* A_)
        : world_{world}
        , src_to_dst_{src_to_dst}
        , A{world.tangent_type(A_,false)}
    {
        // initializes the differentiation for a function of type A -> B
        // src_to_dst expects the parameters of the source lambda to be mapped
        //  (this property is only used later on)

        // the general principle is that every expression is a function
        //   and has a gradient in respect from its outputs to its inputs
        //   for instance add:R²->R has a pullback R->R²
        //   describing how the result depends on the two inputs
        //      (the derivation of the output w.r. to the inputs)
        //   we mostly directly combine building techniques and chain rule applications
        //   into the basic construction to derive the wanted derivative
        //   w.r. to the function inputs of type A for the rev_diff call we currently are working on
        //   in that sense every expression can be seen as a function from function input to some
        //   intermediate result
        //   Therefore, we need to keep track of A (but B is mostly not important)

        // combination of derivatives is in most parts simply multiplication and application
        // the pullbacks handle this for us as the scalar is applied inside the derivative
        // and scales the derivative
        // Therefore, composition of two pullbacks corresponds to (matrix-)multiplication
        // and represents an application of the chain rule
        // the nested nature emulates the backward adjoint trace used in backpropagation
        // also see "Demystifying Differentiable Programming: Shift/Reset the Penultimate Backpropagator"
        // for a similar approach but with shift and reset primitives


        dlog(world_,"Finished Construction");
    }

    const Def* reverse_diff(Lam* src); // top level function to compute the reverse differentiation of a function
private:
    const Def* j_wrap(const Def* def); // 'identity' (except for lambdas, functions, and applications) traversal annotating the pullbacks
    const Def* j_wrap_rop(ROp op, const Def* a, const Def* b); // pullback computation for predefined functions, specifically operations like +, -, *, /
    void derive_external( const Lam* fun, Lam* pb, Lam* fw, Lam* res_lam);
    void derive_numeric( const Lam* fun, Lam* lam_d, const Def* x, r64 delta );

    const Def* seen(const Def* src); // lookup in the map

    // chains cn[:mem, A, cn[:mem, B]] and cn[:mem, B, cn[:mem, C]] to a toplevel cn[:mem, A, cn[:mem, C]]
    const Def* chain(const Def* a, const Def* b);
    const Pi* createPbType(const Def* A, const Def* B);
    const Def* extract_pb(const Def* j_extract);

    World& world_;
    Def2Def src_to_dst_; // mapping old def to new def
    DefMap<const Def*> pullbacks_;  // <- maps a *copied* src term (a dst term) to its pullback function
    DefMap<const Def*> pointer_map;
    const Def* A;// input type

    void initArg(const Def* dst);
    const Def* ptrSlot(const Def* ty, const Def* mem);
    std::pair<const Def*,const Def*> reloadPtrPb(const Def* mem, const Def* ptr, const Def* dbg = {}, bool generateLoadPb=false);

    // next mem object to use / most recent memory object
    // no problem as control flow is handled by cps
    // alternative: j_wrap returns mem object
    // only set at memory alternating operations
    //   load, store, slot, alloc, function arg
    const Def* current_mem;
};


const Def* AutoDiffer::chain(const Def* a, const Def* b) {
    // chaining of two pullbacks is composition due to the
    // nature of a pullback as linear map => application corresponds to (matrix-)multiplication

    auto at = a->type()->as<Pi>();
    auto bt = b->type()->as<Pi>();
    type_dump(world_,"   chain fun a",a);
    type_dump(world_,"   chain fun b",b);

    auto A = at->doms()[1];
    auto B = bt->doms()[1];
    auto C = bt->doms()[2]->as<Pi>()->doms()[1];
    dlog(world_,"   A {}",A);
    dlog(world_,"   B {}",B);
    dlog(world_,"   C {}",C);

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
    // one could keep A "normal" and use tangent type here and at the uses to create a pb ZERO,
    return world_.cn_mem_ret(world_.tangent_type(B,false), A);
}


//const Def* AutoDiffer::extract_pb(const Def* j_tuple, const Def* j_idx) {
const Def* AutoDiffer::extract_pb(const Def* j_extract) {
    if(pullbacks_.count(j_extract))
        return pullbacks_[j_extract];
    auto extract = j_extract->as<Extract>();

    auto pi = createPbType(A,extract->type());
    auto pb = world_.nom_lam(pi, world_.dbg("extract_pb"));
    pb->set_filter(world_.lit_true());
    type_dump(world_,"  pb of extract: ",pb);

    auto [rmem, ohv] = oneHot(world_,pb->mem_var(),extract->index(),world_.tangent_type(extract->tuple()->type(),false),pb->var(1,world_.dbg("s")));

    // or use pullbacsk type
    pb->set_body(world_.app(
        pullbacks_[extract->tuple()],
        {
            rmem,
            ohv,
            pb->ret_var()
        }
    ));
    return pb;
}


// loads pb from shadow slot, updates pb for the ptr, returns, mem and pb for the loaded value
std::pair<const Def*,const Def*> AutoDiffer::reloadPtrPb(const Def* mem, const Def* ptr, const Def* dbg, bool generateLoadPb) {
    auto [pb_load_mem,pb_load_fun] = world_.op_load(mem,pointer_map[ptr],dbg)->projs<2>();
    type_dump(world_,"  reload for ptr",ptr);
    pullbacks_[ptr]=pb_load_fun;
    return {pb_load_mem,pb_load_fun};
}

// top level entry point after creating the AutoDiffer object
// a mapping of source arguments to dst arguments is expected in src_to_dst
const Def* AutoDiffer::reverse_diff(Lam* src) {
    // For each param, create an appropriate pullback. It is just the (one-hot) identity function for each of those.
    type_dump(world_,"Apply RevDiff to src",src);
    current_mem=src_to_dst_[src->mem_var()];
    for(size_t i = 0, e = src->num_vars(); i < e; ++i) {
        auto src_param = src->var(i);
        if(src_param == src->ret_var() || src_param == src->mem_var()) {
            // skip first and last argument
            // memory and return continuation are no "real" arguments
            dlog(world_,"Ignore variable {} of src: {}",i,src_param);
            continue;
        }
        auto dst = src_to_dst_[src_param];
        dlog(world_,"Source Param #{} {} => {} : {}",i,src_param,dst,dst->type());


        // TODO: move computation of A and params here

        size_t dim= getDim(dst->type());
        dlog(world_,"Source Param dim {}",dim);

        // the pullback of the argument with respect to the argument is the identity
        // if the argument is a tuple, each component has a projection of one of the components of the
        // scalar as pullback
        // the scalar chooses which output (component) is under consideration
        auto idpi = createPbType(A,A);
        dlog(world_,"The pullback type of the argument is {}",idpi);
        auto idpb = world_.nom_lam(idpi, world_.dbg("id"));
        idpb->set_filter(world_.lit_true());


        dlog(world_,"Set IDPB");
        // shorten to variable input => id
        idpb->set_body(world_.app(idpb->ret_var(),
            {idpb->mem_var(),idpb->var(1,world_.dbg("s"))}));

        pullbacks_[dst] = idpb;


        initArg(dst);


        type_dump(world_,"Pullback of dst ",pullbacks_[dst]);
    }
    dlog(world_,"Initialization finished, start jwrapping");
    // translate the body => get correct applications of variables using pullbacks
    auto dst = j_wrap(src->body());
    return dst;
}

void AutoDiffer::initArg(const Def* dst) {
    // create shadow slots for pointers


    // we need to initialize the shadow ptr slot for
    // ptr args here instead of at store & load (first usage)
    // as the slot needs the correct pullback (from the ptr object)
    // to be stored and loaded
    // when the ptr shadow slot is accessed it has to have the correct
    // content in the current memory object used to load
    // this is only possible at a common point before all usages
    //   => creation / first mentioning
    auto arg_ty = dst->type();
    dlog(world_,"Arg of Type A: {}", arg_ty);
    if(auto ptr= isa<Tag::Ptr>(arg_ty)) {
        dlog(world_,"Create Ptr arg shadow slot");
        auto ty = ptr->arg()->projs<2>()[0];
        dlog(world_, "A is ptr for {}", ty);

        auto dst_mem = current_mem;
        type_dump(world_, "Dst Mem", dst_mem);
        auto [pb_mem, pb_ptr] = ptrSlot(arg_ty, dst_mem)->projs<2>();
        pointer_map[dst] = pb_ptr;
        type_dump(world_, "Pb Slot", pb_ptr);
        type_dump(world_, "Pb Slot Mem", pb_mem);

        // write the pb into the slot
        auto pb_store_mem = world_.op_store(pb_mem, pb_ptr, pullbacks_[dst], world_.dbg("pb_arg_id_store"));
        type_dump(world_, "Pb Store Mem", pb_store_mem);
        current_mem=pb_store_mem;
        return;
    }



    // prepare extracts

}


const Def* AutoDiffer::ptrSlot(const Def* ty, const Def* mem) {
    auto pbty = createPbType(A,ty);
    //                    auto ptrpbty = createPbType(A,world_.type_ptr(ty));
    auto pb_slot  = world_.op_slot(pbty,mem,world_.dbg("ptr_slot"));
    return pb_slot; // split into pb_mem, pb_ptr
}

void AutoDiffer::derive_numeric( const Lam* fun, Lam* lam_d, const Def* x, r64 delta ){
    // https://www.overleaf.com/read/gdpfxvzqpfjf
    // # Numeric differentiation    for general case

    // d/dx f(x) ≈ (f(x+h/2)-f(x-h/2))/h     (local tangent)
    // or more efficient in multidim: (f(x+h)-f(x))/h

    auto type = x->type();
    auto funType = fun->doms().back()->as<Pi>();
    auto [mem2, half_delta_lit] = lit_of_type(world_, lam_d->mem_var(), type, delta/2, nullptr);
    auto [mem3, delta_lit] = lit_of_type(world_, lam_d->mem_var(), type, delta, nullptr);

    auto high = world_.nom_lam(funType,world_.dbg("high"));
    lam_d->set_body(world_.app(fun, {
            mem3,
            world_.op(ROp::sub, (nat_t)0, x, half_delta_lit),
            high
    }));
    lam_d->set_filter(world_.lit_true());


    auto diff = world_.nom_lam(funType,world_.dbg("low"));
    high->set_body(world_.app(fun, {
            lam_d->mem_var(),
            world_.op(ROp::add, (nat_t)0, x, half_delta_lit),
            diff
    }));
    high->set_filter(world_.lit_true());


    diff->set_body(world_.app(lam_d->ret_var(), {
            high->mem_var(),
            world_.op(ROp::mul, (nat_t)0,
                world_.op(ROp::div, (nat_t)0,
                        world_.op(ROp::sub, (nat_t)0, diff->var(1), high->var(1)),
                        delta_lit
                ),
                lam_d->var(1)
            )
    }));
    diff->set_filter(world_.lit_true());
}


// fills in the body of pb (below called gradlam) which stands for f* the pullback function
// the pullback function takes a tangent scalar and returns the derivative
// fun is the original called external function (like exp, sin, ...) : A->B
// pb is the pullback B->A that might use the argument of fw in its computation
// fw is the new toplevel called function that invokes fun and hands over control to res_lam
// res_lam is a helper function that takes the result f(x) as argument and returns the result together with the pullback
void AutoDiffer::derive_external(const Lam* fun, Lam* pb, Lam* fw, Lam* res_lam){
    std::string name = fun->name();
    // d/dx f(g(x)) = g'(x) f'(g(x))
    // => times s at front

    // x
    const Def* fun_arg = fw->var(1);
    // f(x)
    const Def* res = res_lam->var(1);
    // s (in an isolated environment s=1 -> f*(s) = df/dx)
    const Def* scal = pb->var(1);

    auto user_defined_diff = world_.lookup(name + "_diff");

    // wrapper to add times s around it
    auto scal_mul_wrap =world_.nom_lam(pb->ret_var()->type()->as<Pi>(),world_.dbg("scal_mul"));
    scal_mul_wrap->set_filter(world_.lit_true());
    scal_mul_wrap->set_body(
        world_.app(
            pb->ret_var(),
            {scal_mul_wrap->mem_var(),
                world_.op(ROp::mul, (nat_t) 0, scal, scal_mul_wrap->var(1))
            }
        )
    );

    if(user_defined_diff != nullptr){
        pb->set_body(world_.app(user_defined_diff, {pb->mem_var(), fun_arg, scal_mul_wrap}));
    }else if( name == "log" ){
        const Def* log_type = scal->type();
        auto [rmem,one] = ONE(world_, pb->mem_var(), log_type);

        const Def* log_d = world_.app(pb->ret_var(), {
                rmem,
                world_.op(ROp::div, (nat_t)0, scal, fun_arg)
        });

        pb->set_body(log_d);
    }else if(name == "exp"){
        // d exp(x)/d y = d/dy x * exp(x)
        pb->set_body(
            world_.app(pb->ret_var(),
                {pb->mem_var(),
                    world_.op(ROp::mul, (nat_t)0, res, scal)
               }));
    }else if(name == "sqrt"){
        // d/dx g(sqrt(f(x))) = g'(sqrt(f(x))) * 1/(2sqrt(f(x))) * f'(x)
        // => sqrt(x) |-> lambda s. s/(2res) with res = sqrt(x)
        const Def* real_type = scal->type();
        auto [mem2, two] = lit_of_type(world_,pb->mem_var(), real_type, 2.0,nullptr);
        const Def* log_d = world_.app(pb->ret_var(), {mem2,
              world_.op(ROp::div, (nat_t)0,
                        scal,
                        world_.op(ROp::mul, (nat_t)0, two, res)
                         )
        });

        pb->set_body(log_d);
    }else if(name == "sin"){
        // sin(x) |-> (sin(x), lambda s. s*cos(x))
        auto cos = world_.lookup("cos");

        if(cos == nullptr){
          dlog(world_,"Error: no cos implementation found");
          THORIN_UNREACHABLE;
        }

        pb->set_body(world_.app(cos, {pb->mem_var(), fun_arg, scal_mul_wrap}));
    }else if(name == "cos"){
        // lambda s. -s * sin(x)
        Lam *sin = (Lam*)world_.lookup("sin");

        if(sin == nullptr){
          dlog(world_,"Error: no sin implementation found");
          THORIN_UNREACHABLE;
        }

        auto fun_return_type = fun->doms().back()->as<Pi>();
        auto negate = world_.nom_lam(fun_return_type,world_.dbg("negate"));

        // -s * return of cos
        negate->set_body(world_.app(pb->ret_var(), {
            sin->mem_var(),
            world_.op(ROp::mul, (nat_t)0, negate->var(1), world_.op_rminus((nat_t)0, scal))
        }));
        negate->set_filter(true);

        pb->set_body(world_.app(sin, {pb->mem_var(), fun_arg, negate}));
    }else{
        derive_numeric(fun, pb, fun_arg, 0.001);
    }
    pb->set_filter(world_.lit_true());
}


// implement differentiation for each expression
// an expression is transformed by identity into itself but using the "new" definitions
//   (the correspondence is stored in src_to_dst where needed)
// simultaneously the pullbacks are created and associated in pullbacks_
// lambdas and functions change as returning functions now have an augmented return callback
//   that also takes the continuation for the pullback
//   non-returning functions take an additional pullback for each argument
// the pullbacks are used when passed to the return callbacks and function calls


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
    type_dump(world_,"J_wrap of ",def);
    dlog(world_,"  Node: {}",def->node_name());

    if (auto dst = seen(def)) {
        // we have converted def and already have a pullback
        if(auto m=isa<Tag::Mem>(def->type())) {
            type_dump(world_,"look at mem",def);
            type_dump(world_,"default replacement",dst);
            type_dump(world_,"replace with",current_mem);
            return current_mem;
        }
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

        dlog(world_,"  axiom has tag {}",axiom->tag());
        THORIN_UNREACHABLE;
    }
    if (auto lam = def->isa_nom<Lam>()) {
        // lambda => a function (continuation) (for instance then and else for conditions)
        type_dump(world_,"Lam",lam);
        auto old_pi = lam->type()->as<Pi>();

        auto last_mem=current_mem;

        auto back_order=lam->type()->as<Pi>()->doms().back()->order();
        auto returning = back_order>0;
        dlog(world_,"  lam returning: {}", lam->is_returning());
//        dlog(world_,"  lam returning2: {}", returning);
        dlog(world_,"  order: {}", back_order);
        if(lam->is_returning() || returning) {
            auto dst = world_.op_rev_diff(lam);
            type_dump(world_,"  new lam",dst);
//            THORIN_UNREACHABLE;

            // should not be needed => TODO: handle higher order pb correctly in app
            auto zeropi = createPbType(A,lam->type());
            dlog(world_,"  result: {}",zeropi);
            auto zeropb = world_.nom_lam(zeropi, world_.dbg("zero_pb"));
            type_dump(world_,"  non ret pb (zero)",zeropb);
            zeropb->set_filter(world_.lit_true());
            auto [rmem,zero] = ZERO(world_,zeropb->mem_var(), A);
            zeropb->set_body(world_.app(zeropb->ret_var(), {rmem, zero}));
            pullbacks_[dst] =zeropb;

            return dst;
        }





        dlog(world_,"  lam args {}",old_pi->num_doms());
        auto args = old_pi->num_doms();

        // take a pullback additionally to the argument
        const Pi* pi;
        if(args==1) {
            pi=old_pi;
        }else{
            pi = world_.cn({world_.type_mem(), old_pi->doms()[1], createPbType(A,old_pi->doms()[1])});
        }
        auto dst = world_.nom_lam(pi, world_.dbg(lam->name()));
        type_dump(world_,"  => ",dst);
        src_to_dst_[lam->var()] = dst->var();
        type_dump(world_,"  dst var: ",dst->var());
        if(args>1) {
            pullbacks_[dst->var()] = dst->var(dst->num_vars() - 1); // pullback (for var) is the last argument
            type_dump(world_,"  dst var pb: ",pullbacks_[dst->var()]);
        }
        dst->set_filter(lam->filter());

        current_mem=dst->mem_var();
        dlog(world_,"  set current mem for LamNM {} to {} ", lam,current_mem);
        // same as above: jwrap body
        src_to_dst_[lam] = dst; // in case of mutual/indirect recursion
        auto bdy = j_wrap(lam->body());
        dst->set_body(bdy);

        // TODO: need pb?
//        pullbacks_[dst] = pullbacks_[bdy];
        // never executed but needed for tuple pb
        dlog(world_,"  compute pb ty of lam: {}",lam->type());
        auto zeropi = createPbType(A,lam->type());
        dlog(world_,"  result: {}",zeropi);
        auto zeropb = world_.nom_lam(zeropi, world_.dbg("zero_pb"));
        type_dump(world_,"  non ret pb (zero)",zeropb);
        zeropb->set_filter(world_.lit_true());
        auto [rmem,zero] = ZERO(world_,zeropb->mem_var(), A);
        zeropb->set_body(world_.app(zeropb->ret_var(), {rmem, zero}));
        pullbacks_[dst] =zeropb;



        current_mem=last_mem;
        dlog(world_,"  reset current mem after LamNM {} to {} ",lam,current_mem);
        return dst;
    }
    if (auto glob = def->isa<Global>()) {
        // a global is handled like a ptr slot + store with init
        dlog(world_,"  Global");
        if(auto ptr_ty = isa<Tag::Ptr>(glob->type())) {
            dlog(world_,"  Global Ptr");
            dlog(world_,"  init {}",glob->init());
            auto dinit = j_wrap(glob->init());
            auto dst=world_.global(dinit,glob->is_mutable(),glob->dbg());

            auto pb = pullbacks_[dinit];
            type_dump(world_,"  pb for global init ",pb);

            auto [ty,addr_space] = ptr_ty->arg()->projs<2>();
            type_dump(world_,"  ty",ty);

            auto [pb_mem, pb_ptr] = ptrSlot(ty,current_mem)->projs<2>();
            pointer_map[dst]=pb_ptr;
            auto pb_mem2 = world_.op_store(pb_mem,pb_ptr,pb,world_.dbg("pb_global"));

            auto [pbt_mem,pbt_pb]= reloadPtrPb(pb_mem2,dst,world_.dbg("ptr_slot_pb_loadS"),false);

            current_mem=pbt_mem;

            type_dump(world_,"  pb slot global ",pb_ptr);
            src_to_dst_[glob]=dst;
            return dst;
        }
    }

    // handle operations in a hardcoded way
    // we directly implement the pullbacks including the chaining w.r. to the inputs of the function
    if (auto rop = isa<Tag::ROp>(def)) {
        type_dump(world_,"  ROp",rop);
        auto ab = j_wrap(rop->arg());
        type_dump(world_,"  args jwrap",ab);
        auto [a, b] = ab->projs<2>();
        type_dump(world_,"  arg a",a);
        type_dump(world_,"  arg b",b);
        if(!pullbacks_.count(a)) {
            pullbacks_[a]= extract_pb(a);
            type_dump(world_,"  created pb for a",pullbacks_[a]);
            pullbacks_[b]= extract_pb(b);
            type_dump(world_,"  created pb for b",pullbacks_[b]);
        }
        auto dst = j_wrap_rop(ROp(rop.flags()), a, b);
        src_to_dst_[rop] = dst;
        type_dump(world_,"  result of app",dst);
        return dst;
    }
    // conditionals are transformed by the identity (no pullback needed)
    if(auto rcmp = isa<Tag::RCmp>(def)) {
        type_dump(world_,"  RCmp",rcmp);
        auto ab = j_wrap(rcmp->arg());
        type_dump(world_,"  args jwrap",ab);
        auto [a, b] = ab->projs<2>();
        auto dst = world_.op(RCmp(rcmp.flags()), nat_t(0), a, b);
        src_to_dst_[rcmp] = dst;
        type_dump(world_,"  result of app",dst);
        return dst;
    }

    if (auto div = isa<Tag::Div>(def)) {
        // only on integer => no pullback needed
        type_dump(world_,"  DIVISION",div);
        auto args = j_wrap(div->arg());
        type_dump(world_,"  Division org args:",div->arg());
        type_dump(world_,"  Division wrapped args:",args);
        type_dump(world_,"  Division callee:",div->callee());
        auto dst = world_.app(div->callee(),args);
        pullbacks_[dst]=pullbacks_[args->op(1)]; // the arguments are (mem, int, int)
        return dst;
    }
    if(auto cast = isa<Tag::Bitcast>(def)) {
        // TODO: handle more than identity bitcast
        type_dump(world_,"  Bitcast:",cast);
        auto args = j_wrap(cast->arg());
        type_dump(world_,"  Bitcast:",cast);
        type_dump(world_,"  Bitcast arg:",cast->arg());
        type_dump(world_,"  Wraped Bitcast args:",args);
        // avoid case distinction
        auto dst = world_.app(cast->callee(),args);
        type_dump(world_,"  Wraped Bitcast:",dst);
        // a zero pb but do not recompute
        pullbacks_[dst]=pullbacks_[args];
        return dst;
    }
    if(auto iop = isa<Tag::Conv>(def)) {
        // Unify with wrap
        type_dump(world_,"  Conv:",iop);
        auto args = j_wrap(iop->arg());
        type_dump(world_,"  Wraped Conv args:",args);
        // avoid case distinction
        auto dst = world_.app(iop->callee(),args);
        type_dump(world_,"  Wraped Conv:",dst);
        // a zero pb but do not recompute
        pullbacks_[dst]=pullbacks_[args];
        return dst;
    }
    if(auto iop = isa<Tag::Wrap>(def)) {
        type_dump(world_,"  Wrap:",iop);
        auto args = j_wrap(iop->arg());
        type_dump(world_,"  Wraped Wrap args:",args);
        // avoid case distinction
        auto dst = world_.app(iop->callee(),args);
        type_dump(world_,"  Wraped Wrap:",dst);
        // a zero pb but do not recompute
        pullbacks_[dst]=pullbacks_[args->op(0)];
        return dst;
    }
    // TODO: more general integer operations
    if(auto icmp = isa<Tag::ICmp>(def)) {
        type_dump(world_,"  ICmp",icmp);
        auto ab = j_wrap(icmp->arg());
        auto [a, b] = ab->projs<2>();
        auto dst = world_.op(ICmp(icmp.flags()), a, b);
        src_to_dst_[icmp] = dst;
        type_dump(world_,"  result of app",dst);
        return dst;
    }
    if (auto alloc = isa<Tag::Alloc>(def)) {
        type_dump(world_,"  Alloc",alloc);
        type_dump(world_,"  alloc mem arg",alloc->arg()); // mem
        type_dump(world_,"  alloc type",alloc->type());
        // inner callee type:  array: size; type
        type_dump(world_,"  alloc callee",alloc->callee()); // Tuple first is type, second gid

        auto alloc_arg = alloc->callee()->as<App>()->arg();
        type_dump(world_,"  alloc arg",alloc_arg);
        auto [base_type,gid] = alloc_arg->projs<2>();
        auto [_,ptr_type]=alloc->type()->projs<2>();
        type_dump(world_,"  alloc base type",base_type);
        type_dump(world_,"  alloc ptr type",ptr_type);
        auto type=base_type;
        type_dump(world_,"  alloc inner type",type);

        auto mem_arg = j_wrap(alloc->arg());

        auto dst = world_.op_alloc(type,mem_arg,alloc->dbg());
        auto [r_mem,arr] = dst->projs<2>();
        type_dump(world_,"  orig alloc",alloc);
        type_dump(world_,"  dst",dst);
        type_dump(world_,"  arr",arr);

        auto pb_ty = createPbType(A,ptr_type);
        type_dump(world_,"  pb_ty",pb_ty);

        // no shadow needed
        // TODO: shadow if one handles alloc like a ptr (for definite)
        auto pb = world_.nom_lam(pb_ty, world_.dbg("pb_alloc"));
        pb->set_filter(world_.lit_true());
        auto [z_mem,z] = ZERO(world_,pb->mem_var(),A);
        pb->set_body( world_.app(pb->ret_var(), {z_mem,z}));

        current_mem=r_mem;
        pullbacks_[arr]=pb;
        pullbacks_[dst]=pullbacks_[arr]; // for call f(rmem, arr)

        src_to_dst_[alloc]=dst;
        return dst;
    }
    if (auto lea = isa<Tag::LEA>(def)) {
        // Problems:
        //   we want a shadow cell for the resulting ptr
        //   but we need a memory to create a slot
        //     slot creation location does not matter => use src mem
        //     (alternative: create slots at start)
        //   => not possible as we need to embed the resulting mem

        // Problem: The shadow slot needs correct pb for the
        //   array element

        // we can not move the shadow slot & its store into the pb (same reason as for ptr)

        dlog(world_,"  Lea");
        dlog(world_,"  projs: {}",lea->projs());
        dlog(world_,"  args: {}",lea->args());
        dlog(world_,"  type: {}",lea->type());
        dlog(world_,"  callee type: {}",lea->callee_type());
        auto ptr_ty = as<Tag::Ptr>(lea->type());
        auto [ty,addr_space] = ptr_ty->arg()->projs<2>();
        dlog(world_,"  inner type: {}", ty);

        auto arr = j_wrap(lea->arg(0));
        auto idx = j_wrap(lea->arg(1)); // not necessary
        auto dst = world_.op_lea(arr,idx);

        type_dump(world_,"  lea arr:", arr);
        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr->type())->arg()->projs<2>();

        auto pi = createPbType(A,ty);
        auto pb = world_.nom_lam(pi, world_.dbg("pb_lea"));
        pb->set_filter(world_.lit_true());

        auto [mem2,ptr_arr] = world_.op_alloc(arr_ty,pb->mem_var())->projs<2>();
        auto scal_ptr = world_.op_lea(ptr_arr,idx);
        auto mem3=mem2;
        auto v = pb->var(1);
        auto mem4 = world_.op_store(mem3,scal_ptr,v);
        type_dump(world_,"ptr_arr",ptr_arr);

        assert(pullbacks_.count(arr) && "arr from lea should already have an pullback");

        pb->set_body( world_.app(
            pullbacks_[arr],
            {
                mem4,
                ptr_arr,
                pb->ret_var()
            }
            ));


        auto [cmem2,ptr_slot]=world_.op_slot(pb->type(),current_mem,world_.dbg("lea_ptr_shadow_slot"))->projs<2>();
        auto cmem3=world_.op_store(cmem2,ptr_slot,pb);
        pointer_map[dst]=ptr_slot;

        // instead of reload because we have no toplevel mem here
        // and this point dominates all usages

        auto [cmem4, _]= reloadPtrPb(cmem3,dst,world_.dbg("lea_shadow_load"),false);
        current_mem=cmem4;

        // in a structure preseving setting
        //   meaning diff of tuple is tuple, ...
        //   this would be a lea

        return dst;
    }

    // memory operations

    // there are many ways to handle memory but most have problems
    // the pullback for the pointer only gets a meaning at a store
    // but the store is only related to the memory
    // we could compute the derivation value w.r. to the pointer but we need
    // the pullback of the pointer w.r. to the inputs at the point of a load
    // therefore, the pointer needs a reference to the pullback of the value
    // assigned at a store
    // the pullback is statically unknown as the control flow determines which
    // store is taken

    // we propagate the memory from before to pullback calls to the transformed dst calls to after
//    if(auto slot = isa<Tag::Slot>(def)) {
//
//    }



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
            dlog(world_,"  app of app");
            // Take care of binary operations

            type_dump(world_, "  inner callee", inner->callee());
            dlog(world_, "  node name {}", inner->callee()->node_name());
            if (auto inner2_app = inner->callee()->isa<App>()) {
                dlog(world_, "  app of app of app");
                if(auto axiom = inner2_app->callee()->isa<Axiom>(); axiom && axiom->tag()==Tag::RevDiff) {
                    auto d_arg = j_wrap(arg); // args to call diffed function
                    auto fn = inner->arg(); // function to diff
                    // inner2_app = rev_diff <...>
                    // callee = rev_diff ... fun
                    auto dst = world_.app(callee,d_arg);
                    type_dump(world_, "  translated to ",dst);
                    src_to_dst_[app]=dst;
                    return dst;
                }
            }

            if (auto axiom = inner->callee()->isa<Axiom>()) {
                dlog(world_,"  app of axiom [...] args with axiom tag {}",axiom->tag());

                if (axiom->tag() == Tag::Slot) {
                    type_dump(world_,"  wrap slot with args ",arg);
                    type_dump(world_,"  wrap slot with inner args ",inner->arg());
                    auto [ty, addr_space] = inner->arg()->projs<2>();
                    auto j_args = j_wrap(arg);
                    auto [mem, num] = j_args->projs<2>();

                    auto [pb_mem, pb_ptr] = ptrSlot(ty,mem)->projs<2>();

                    auto dst = world_.op_slot(ty,pb_mem);
                    auto [dst_mem, dst_ptr] = dst->projs<2>();
                    type_dump(world_,"  slot dst ptr",dst_ptr);
                    type_dump(world_,"  slot pb ptr",pb_ptr);

                    pointer_map[dst]=pb_ptr; // for mem tuple extract
                    pointer_map[dst_ptr]=pb_ptr;

                    type_dump(world_,"  result slot ",dst);
                    type_dump(world_,"  pb slot ptr ",pb_ptr);
                    src_to_dst_[app] = dst; // not needed
                    current_mem=dst_mem;
                    return dst;
                }
                if (axiom->tag() == Tag::Store) {
                    type_dump(world_,"  wrap store with args ",arg);
                    type_dump(world_,"  wrap store with inner args ",inner->arg());
                    auto j_args = j_wrap(arg);
                    type_dump(world_,"  continue with store with args ",j_args);

                    auto [mem, ptr, val] = j_args->projs<3>();
                    type_dump(world_,"  got ptr at store ",ptr);

                    assert(pointer_map.count(ptr) && "ptr should have a shadow slot at a store location");

                    type_dump(world_,"  got ptr pb slot ",pointer_map[ptr]);
                    type_dump(world_,"  got val ",val);

                    auto pb=pullbacks_[val];

                    auto pb_mem = world_.op_store(mem,pointer_map[ptr],pb,world_.dbg("pb_store"));

                    // necessary to access ptr pb when calling
                    // all other accesses are handled by load of the ptr with corresponding pb slot load
                    auto [pbt_mem,pbt_pb]= reloadPtrPb(pb_mem,ptr,world_.dbg("ptr_slot_pb_loadS"),false);
                    type_dump(world_,"  store loaded pb fun",pullbacks_[ptr]);

                    auto dst = world_.op_store(pbt_mem,ptr,val);
                    type_dump(world_,"  result store ",dst);
                    type_dump(world_,"  pb store ",pb_mem);
                    pullbacks_[dst]=pb; // should be unused
                    src_to_dst_[app] = dst; // not needed
                    current_mem=dst;
                    return dst;
                }
                if (axiom->tag() == Tag::Load) {
                    type_dump(world_,"  wrap load with args ",arg);
                    type_dump(world_,"  wrap load with inner args ",inner->arg());

                    auto j_args = j_wrap(arg);
                    type_dump(world_,"  continue with load with args ",j_args);

                    auto [mem, ptr] = j_args->projs<2>();
                    type_dump(world_,"  got ptr at load ",ptr);

                    dlog(world_,"has ptr in pb {}",pullbacks_.count(ptr));

                    // TODO: where is pullbacks_[ptr] set to a nullptr? (happens in conditional stores to slot)
                    // TODO: why do we need or not need this load
                        dlog(world_,"manually load ptr pb at load location");
                        auto [nmem,pb_loaded]=reloadPtrPb(mem,ptr,world_.dbg("ptr_slot_pb_loadL"),true);
                        mem=nmem;

                    dlog(world_,"  got ptr pb {} ",pullbacks_[ptr]);
                    type_dump(world_,"  got ptr pb ",pullbacks_[ptr]);

                    auto dst = world_.op_load(mem,ptr);
                    auto [dst_mem,dst_val] = dst->projs<2>();

                    type_dump(world_,"  result load ",dst);
                    pullbacks_[dst]=pb_loaded; // tuple extract [mem,...]
                    src_to_dst_[app] = dst; // not needed except
                    current_mem=dst_mem;
                    return dst;
                }
            }
        }


        // distinguish between returning calls (other functions)
        // and non-returning calls (give away control flow) for instance for conditionals

        // a returning call is transformed using rev_diff with another rewrite pass
        // a non-returning call is transformed directly and augmented using pullbacks for its arguments

        auto back_order=callee->type()->as<Pi>()->doms().back()->order();
        auto returning = back_order>0;
        if (callee->type()->as<Pi>()->is_returning() || returning) {
            dlog(world_,"  FYI returning callee");

            const Def* dst_callee;

            auto d_arg = j_wrap(arg);
            type_dump(world_,"  wrapped args: ",d_arg);

            if(auto cal_lam=callee->isa<Lam>(); cal_lam && !cal_lam->is_set()) {
                dlog(world_,"  found external function");
                dlog(world_,"  function name {}",cal_lam->name());

                // derive the correct type for the differentiated function f'
                // f'(x) = (f(x), f*)
                // where f*(1) = df/dx

                // idea in pseudocode:
                // f is eta convertible to λ mem arg ret. f (mem,arg,ret)
                // we want to intercept and also return the gradient
                // f: A -> B
                //  = cn[mem, A, cn[mem, B]]
                // f'
                // lam₁ = λ mem arg ret. f (mem,arg,lam₂)
                //      = x ↦ lam₂(f(x))
                //    : A -> B*(B->A)
                //      = cn[mem, A, cn[mem, B, cn[mem, B, cn[mem, A]]]]
                // 
                // lam₂ = λ mem₂ res. ret (mem₂, res, grad)
                //      = y ↦ (y,grad(x))
                //    : B -> B*(B->A)
                //      = cn[mem, B]
                //  res is f(x)
                //  lam₂ might look returning in its body but it takes not returning argument
                //   instead it uses the return from lam₁ which is the return supplied by the user
                // 
                // f*
                // grad = λ x. λ mem s ret. ...
                //    : A -> (B -> A)
                //      = A -> cn[mem, B, cn[mem, A]]
                //  x is supplied at compile time by direct forwarding from lam₁

                auto augTy = world_.tangent_type(callee->type(),true)->as<Pi>();
                // type of result (after taking argument x)
                auto resTy = augTy->doms().back()->as<Pi>();
                // type of the pullback f*
                auto pbTy = resTy->doms().back()->as<Pi>();

                dlog(world_,"  augmented ty {}", augTy);
                dlog(world_,"  result {}", resTy);
                dlog(world_,"  pullback type {}", pbTy);

                // f*
                auto gradlam=world_.nom_lam(pbTy, world_.dbg("dummy"));

                // new augmented lam f' to replace old one
                auto lam=world_.nom_lam(augTy,world_.dbg("dummy"));
                dlog(world_,"lam2 ty {}",cal_lam->doms().back());
                dlog(world_,"lam2 ty {}",cal_lam->doms().back()->as<Pi>());
                auto lam2 = world_.nom_lam(cal_lam->doms().back()->as<Pi>(),world_.dbg("dummy"));

                derive_external(cal_lam, gradlam, lam, lam2);

                lam->set_name(cal_lam->name() + "_diff_impl");
                lam2->set_name(lam->name() + "_cont");
                gradlam->set_name(cal_lam->name() + "_pb");
                dlog(world_,"isset grad {}",gradlam->is_set());

                lam->set_body( world_.app(
                    callee,
                    {
                        lam->mem_var(),
                        lam->var(1),
                        lam2
                    }
                ));
                lam->set_filter(world_.lit_true());

                lam2->set_body( world_.app(
                    lam->ret_var(),
                    {
                        lam2->mem_var(),
                        lam2->var(1),
                        gradlam
                    }
                ));
                lam2->set_filter(world_.lit_true());


                type_dump(world_,"new lam",lam);
                type_dump(world_,"aux lam",lam2);
                type_dump(world_,"grad lam",gradlam);

                dst_callee = lam;
            }else {
                type_dump(world_,"  fn callee",callee);
                dlog(world_,"  fn callee node {}",callee->node_name());
                if(callee->isa<Lam>()) {
                    dlog(world_,"  op_rev_diff function");
                    auto ret_ty = callee->type()->as<Pi>()->doms().back()->as<Pi>();
                    dlog(world_,"  ret_ty {}",ret_ty);
                    dlog(world_,"  ret_ty num doms {}",ret_ty->num_doms());
                    if(ret_ty->num_doms()==1) {
                        // function is cn[mem] => only side effects
                        // and it is a called function
                        // => do nothing
                        dlog(world_,"  void returning function");
                        auto dst = world_.app(
                           callee,
                            d_arg
                        );
                        pullbacks_[dst] = pullbacks_[d_arg];
                        return dst;
                    }else {
                        dst_callee = world_.op_rev_diff(callee);
                        type_dump(world_,"  Used RevDiff Op on callee",dst_callee);
                        dlog(world_,"  this call will invoke AutoDiff rewrite");
                    }
                }else{
                    dlog(world_,"  j_wrap argument");
                    dst_callee= j_wrap(callee);
                    type_dump(world_,"  j_wrap callee (for higher order)",dst_callee);
                }
            }


            auto [m,arg,ret_arg] = d_arg->projs<3>();
            type_dump(world_,"  split wrapped args into: mem: ",m);
            type_dump(world_,"  split wrapped args into: arg: ",arg);
            type_dump(world_,"  split wrapped args into: ret: ",ret_arg);

            auto pbT = dst_callee->type()->as<Pi>()->doms().back()->as<Pi>();
            auto chained = world_.nom_lam(pbT, world_.dbg("φchain"));
            type_dump(world_,"  orig callee",callee);
            type_dump(world_,"  dst callee",dst_callee);
            type_dump(world_,"  chained pb will be (app pb) ",chained);

            dlog(world_,"  d_arg {}",d_arg);
            dlog(world_,"  d_arg pb {}",pullbacks_[d_arg]);

            auto arg_pb = pullbacks_[d_arg]; // Lam
            type_dump(world_,"  arg pb",arg_pb);
            auto ret_pb = chained->ret_var(); // extract
            type_dump(world_,"  ret var pb",ret_pb);
            auto chain_pb = chain(ret_pb,arg_pb);
            type_dump(world_,"  chain pb",chain_pb);


            chained->set_body( world_.app(
                ret_arg,
                {
                    chained->mem_var(),
                    chained->var(1),
                    chain_pb
                }
                ));
            chained->set_filter(world_.lit_true());
            type_dump(world_,"  build chained (app pb) ",chained);

            auto dst = world_.app(dst_callee, {m,arg,chained});

            type_dump(world_,"  application with jwrapped args",dst);

            pullbacks_[dst] = pullbacks_[d_arg];
            type_dump(world_,"  pullback of dst (call app): ",pullbacks_[dst]);

            return dst;
        }else {
            dlog(world_,"  FYI non-returning callee");
            auto d_arg = j_wrap(arg);
            auto d_callee= j_wrap(callee); // invokes lambda
            type_dump(world_,"  wrapped callee: ",d_callee);
            type_dump(world_,"  wrapped args: ",d_arg);
            dlog(world_,"  arg in pb: {}",pullbacks_.count(d_arg));
            if(pullbacks_.count(d_arg))
                type_dump(world_,"  arg pb: ",pullbacks_[d_arg]);
            dlog(world_,"  type: {}",d_arg->node_name());
            const Def* ad_args;

            dlog(world_,"  arg type: {} of {}",d_arg->type(),d_arg->type()->node_name());


            // if we encounter a tuple (like [mem, arg]) we add the pullback as additional argument
            // this is necessary for lambdas (conditionals)
            // as well as for the final return, which expects [mem, result, pullback of result w.r. to inputs]
            // all tuples are sigma types
            // one problem: if we have continuation calls (for instance with conditionals),
            //   we transformed their signature to take the pullback
            //   if this continuation makes a non-returning call with [mem,arg] in the normal form
            //   lazy code is generated to forward all arguments
            //   this results in forwarding the pullback as well
            //   therefore, we do not need to additionally give the pullback
            //   (which in the code would rather result in omitting the main argument due to wrong counting of arguments)
            //   thus, we skip the augmentation when encountering a var => an argument which is the whole argument of a function call
            // another case where no agumentation is needed is when a function with only one mem argument
            //   is called (like in conditionals)
            //   we have no pullback => no augmentation needed
            //   coincidentally, this is covered by !type->is<Sigma>() as well as darg->is<Var>

            if(d_arg->type()->isa<Sigma>() && !d_arg->isa<Var>()) {
                dlog(world_,"  tuple argument");
                auto count=getDim(d_arg);
                dlog(world_,"  count: {}",count);
                ad_args = world_.tuple(
                    Array<const Def*>(
                    count+1,
                    [&](auto i) {if (i<count) {return world_.extract(d_arg, (u64)i, world_.dbg("ad_arg"));} else {return pullbacks_[d_arg];}}
                ));
            }else {
                // var (lambda completely with all arguments) and other (non tuple)
                dlog(world_,"  non tuple argument");
                ad_args = d_arg;
            }
            type_dump(world_,"  ad_arg ",ad_args);
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
        auto tuple_dim=getDim(tuple->type());
        dlog(world_,"  num of ops: {}",tuple_dim);
        // jwrap each component
        Array<const Def*> ops{tuple_dim, [&](auto i) { return j_wrap(tuple->proj(i)); }};
        dlog(world_,"  jwrapped elements: {, }",ops);
        if(tuple_dim>0 && isa<Tag::Mem>(tuple->proj(0)->type())) {
            ops[0] = j_wrap(tuple->proj(0));
        }
        // reconstruct the tuple term
        auto dst = world_.tuple(ops);
        type_dump(world_,"  tuple:",tuple);
        type_dump(world_,"  jwrapped tuple:",dst);
        src_to_dst_[tuple] = dst;

        if(tuple_dim>0 && isa<Tag::Mem>(dst->proj(0)->type())) {
            dlog(world_,"  mem pb tuple");
            if(tuple_dim>1)
                pullbacks_[dst] = pullbacks_[ops[1]];
            return dst;
        }


        dlog(world_,"tangent type of tuple: {} => {}",tuple->type(),world_.tangent_type(tuple->type(),false));
        dlog(world_,"tangent type of dst: {} => {}",dst->type(),world_.tangent_type(dst->type(),false));
        dlog(world_,"tuple dim: {}",tuple_dim);


        // TODO: simplify
        // TODO: could a more modular approach with more primitive pullbacks make this code easier?

        // get pullbacks for each component w.r. to A
        // apply them with the component of the scalar from the tuple pullback
        // sum them up

        auto pi = createPbType(A,tuple->type());
        auto pb = world_.nom_lam(pi, world_.dbg("tuple_pb"));
        dlog(world_,"  complete tuple pb type: {}",pi);
        pb->set_filter(world_.lit_true());

        type_dump(world_,"  A:",A);
        auto pbT = pi->as<Pi>()->doms().back()->as<Pi>();
        dlog(world_,"  intermediate tuple pb type: {}",pbT);
        dlog(world_,"  should be cn_mem of {}",A);
        auto cpb = pb;
        auto [cpb_mem,sum]=ZERO(world_,cpb->mem_var(),A);
        Lam* nextpb;

        for (size_t i = 0; i < tuple_dim; ++i) {
            nextpb = world_.nom_lam(pbT, world_.dbg("φtuple_next"));
            nextpb->set_filter(world_.lit_true());
            dlog(world_,"    build zeroPB op {}: {} : {}",i,ops[i],ops[i]->type());
            dlog(world_,"      pb {} : {}",pullbacks_[ops[i]],pullbacks_[ops[i]]->type());
            dlog(world_,"      pb var: {}:{}",
                    world_.extract_unsafe(pb->var(1, world_.dbg("s")), i),
                    world_.extract_unsafe(pb->var(1, world_.dbg("s")), i)->type());
            cpb->set_body(
                world_.app(pullbacks_[ops[i]],
                    {cpb_mem,
                    world_.extract_unsafe(pb->var(1, world_.dbg("s")), i),
                    nextpb
                    }));
            cpb=nextpb;
            cpb_mem=cpb->mem_var();
            //all nextpb args are result
            auto [nmem, nsum]=vec_add(world_,cpb_mem,sum,nextpb->var(1));
            cpb_mem=nmem;
            sum=nsum;
        }
        dlog(world_,"  create final pb app");
        cpb->set_body( world_.app( pb->ret_var(), {cpb_mem,sum} ));

        // TODO: multiple arguments

        dlog(world_,"  tuple pbs {}",pb);
        pullbacks_[dst]=pb;
        type_dump(world_,"  pullback for tuple",pullbacks_[dst]);
        return dst;
    }

    if (auto pack = def->isa<Pack>()) {
        // no pullback for pack needed
        type_dump(world_,"Pack",pack);
        auto d_bdy=j_wrap(pack->body());
        auto dst = world_.pack(pack->type()->arity(), d_bdy);
        src_to_dst_[pack] = dst;

        // TODO: a pack can only be extracted => optimize
        // TODO: handle non-lit arity (even possible?)
        // TODO: unify with tuple
        auto dim = as_lit(pack->type()->arity());

            auto pi = createPbType(A,dst->type());
        auto pb = world_.nom_lam(pi, world_.dbg("pack_pb"));
        dlog(world_,"  complete pack pb type: {}",pi);
        pb->set_filter(world_.lit_true());

        auto pbT = pi->as<Pi>()->doms().back()->as<Pi>();
        dlog(world_,"  intermediate pack pb type: {}",pbT);
        auto cpb = pb;
        auto [cpb_mem,sum]=ZERO(world_,cpb->mem_var(),A);
        Lam* nextpb;

        // TODO: Same sum complication as for tuples
        for (size_t i = 0; i < dim; ++i) {
            nextpb = world_.nom_lam(pbT, world_.dbg("φpack_next"));
            nextpb->set_filter(world_.lit_true());
            cpb->set_body(
                world_.app(pullbacks_[d_bdy],
                           {cpb_mem,
                            world_.extract_unsafe(pb->var(1, world_.dbg("s")), i),
                            nextpb
                           }));
            cpb=nextpb;
            cpb_mem=cpb->mem_var();
            auto [nmem, nsum]=vec_add(world_,cpb_mem,sum,nextpb->var(1));
            cpb_mem=nmem;
            sum=nsum;
        }
        dlog(world_,"  create final pb app");
        cpb->set_body( world_.app( pb->ret_var(), {cpb_mem,sum} ));

        dlog(world_,"  pack pbs {}",pb);
        pullbacks_[dst]=pb;

        type_dump(world_,"  jwrapped pack",dst);
        return dst;
    }

    if (auto extract = def->isa<Extract>()) {
        // extracting a tuple B^m results in element B
        // the tuple has a pullback B^m->A (remember the tuple is viewed as function in the inputs)
        // to get the pullback for the i-th argument
        // we have to apply the pullback with the one-hot vector with a 1 (or rather s) at position i
        // but the extraction position is not statically known therefore, we can not
        // directly convert the extraction index to a position in a tuple
        // thus, we need to list all one-hot vectors in a tuple and extract the correct one
        // using the extraction index
        // this extracted one-hot vector can now be used to be applied to the pullback of the tuple
        // to project the correct gradient


        // when extracting a component, the pullback is extracted from the tuple pullback of the tuple argument
        type_dump(world_,"Extract",extract);
        type_dump(world_,"  extract idx",extract->index());
        auto jeidx= j_wrap(extract->index());
        type_dump(world_,"  extract wrapped idx",jeidx);

        auto jtup = j_wrap(extract->tuple());
        type_dump(world_,"  original extract",extract);
        type_dump(world_,"  original tuple",extract->tuple());
        type_dump(world_,"  jwrapped tuple of extract",jtup);

        auto dst = world_.extract_unsafe(jtup, jeidx);
        type_dump(world_,"  jwrapped extract",dst);
        src_to_dst_[extract] = dst;
        // do not extract diff
        // but tuple => tuple of diffs
        // no lambda

        // TODO: more general handling of memory
        if(isa<Tag::Mem>(jtup->type()->proj(0))) {
            dlog(world_,"  extract mem pb tuple ");

            // for special case pointer slot that has not yet be written to
            if(pullbacks_.count(jtup) && ! isa<Tag::Mem>(dst->type())) {
                pullbacks_[dst] = pullbacks_[jtup];
                assert(pullbacks_[jtup] && "Tuple that is extracted should have pullback.");
                type_dump(world_,"  pullback of extract",pullbacks_[dst]);
            }
            return dst;
        }


        pullbacks_[dst] = extract_pb(dst);
        type_dump(world_,"  pullback of extract",pullbacks_[dst]);
        return dst;
    }

    if (auto insert = def->isa<Insert>()) {
        // TODO: currently not handled but not difficult
        // important note: we need the pullback w.r. to the tuple and element
        // construction needs careful consideration of modular basic pullbacks
        // see notes on paper for correct code

        type_dump(world_,"Insert",insert);
        auto dst = world_.insert(j_wrap(insert->tuple()), insert->index(), j_wrap(insert->value()));
        src_to_dst_[insert] = dst;
        type_dump(world_,"  jwrapped insert",dst);
        dlog(world_,"  TODO: pullback of insert is currently missing");
        return dst;
    }

    if (auto lit = def->isa<Lit>()) {
        // a literal (number) has a zero pullback
        type_dump(world_,"Literal",lit);
        auto zeropi = createPbType(A,lit->type());
        auto zeropb = world_.nom_lam(zeropi, world_.dbg("zero_pb"));
        type_dump(world_,"  lit pb (zero)",zeropb);
        zeropb->set_filter(world_.lit_true());
        auto [rmem,zero] = ZERO(world_,zeropb->mem_var(), A);
        dlog(world_,"  computed zero");

        dlog(world_,"  zeropb retvar {}",zeropb->ret_var());
        type_dump(world_,"  rmem",rmem);
        dlog(world_,"  zero: {} ",zero);
        type_dump(world_,"  zero",zero);
        zeropb->set_body(world_.app(zeropb->ret_var(), {rmem, zero}));
        // no src_to_dst mapping necessary
        pullbacks_[lit] = zeropb;
        dlog(world_,"  set zero pb");
        return lit;
    }

    type_dump(world_,"unhandeled def",def);
    dlog(world_,"  node {}",def->node_name());
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

            pb->set_body(world_.app(apb, {pb->mem_var(), pb->var(1), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), pb->var(1), end}));
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto [smem, sum] = vec_add(world_, end->mem_var(), adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { smem, sum}));
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

            pb->set_body(world_.app(apb, {pb->mem_var(), pb->var(1), middle}));
            auto [rmem,one] = ONE(world_,middle->mem_var(), o_type);
            middle->set_body(world_.app(bpb, {rmem, world_.op(ROp::mul, (nat_t)0, pb->var(1), world_.op_rminus((nat_t)0, one)), end}));
            // all args 1..n as tuple => vector for addition
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto [smem, sum] = vec_add(world_, end->mem_var(), adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { smem, sum}));
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

            auto [smem, sum] = vec_add(world_, end->mem_var(), adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { smem, sum}));
            pullbacks_[dst] = pb;
            return dst;
        }
            // ∇(a / b) = λz. (g* (z * h) - h* (z * g))/h²
        case ROp::div: {
            //    a*(1/b * z)          => a*(z/b)
            //  + b*(a * -b^(-2) * z)  => b*(-z*a/(b*b))
            auto dst = world_.op(ROp::div, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "/"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::div, (nat_t)0, pb->var(1), b), middle}));
            auto za=world_.op(ROp::mul, (nat_t)0, pb->var(1), a);
            auto bsq=world_.op(ROp::mul, (nat_t)0, b, b);
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op_rminus((nat_t)0, world_.op(ROp::div, (nat_t)0, za, bsq)), end}));
            auto adiff = middle->var(1);
            auto bdiff = end->var(1);

            auto [smem, sum] = vec_add(world_, end->mem_var(), adiff, bdiff);
            end->set_body(world_.app(pb->ret_var(), { smem, sum}));
            pullbacks_[dst] = pb;
            return dst;
        }
        default:
            // only +, -, *, / are implemented as basic operations
            THORIN_UNREACHABLE;
    }
}

// seen is a simple lookup in the src_to_dst mapping
const Def* AutoDiffer::seen(const Def* src) { return src_to_dst_.contains(src) ? src_to_dst_[src] : nullptr; }

} // namespace

// rewrites applications of the form 'rev_diff function' into the differentiation of f
const Def* AutoDiff::rewrite(const Def* def) {
    // isa<Tag::RevDiff> is not applicable here
    if (auto app = def->isa<App>()) {
        if (auto type_app = app->callee()->isa<App>()) {
            if (auto axiom = type_app->callee()->isa<Axiom>(); axiom && axiom->tag() == Tag::RevDiff) {
        // rev_diff(f)
        // in thorin :rev_diff ‹2∷nat; r32› f
        //           --------- app ----------
        //           ------ type_app ------ arg
        //           (axiom    arg2       ) arg

        auto src_lam = app->arg(0)->as_nom<Lam>();//->as_nom<Lam>();
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


        dlog(world,"AD of function from {} to {}",A,B);
        type_dump(world,"Transform:",src_lam);
        type_dump(world,"Result:",dst_lam);

        // The actual AD, i.e. construct "sq_cpy"
        Def2Def src_to_dst;
        // src_to_dst maps old definitions to new ones
        // here we map the arguments of the lambda
        for (size_t i = 0, e = src_lam->num_vars(); i < e; ++i) {
            auto src_param = src_lam->var(i);
            auto dst_param = dst_lam->var(i, world.dbg(src_param->name()));
            // the return continuation changes => special case
            src_to_dst[src_param] = dst_param;
        }
        auto differ = AutoDiffer{world, src_to_dst, A};
        dst_lam->set_body(differ.reverse_diff(src_lam));


        return dst_lam;
    }}}
    return def;
}

}