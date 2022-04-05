#include "thorin/pass/rw/auto_diff.h"

#include <algorithm>
#include <string>

#include "thorin/analyses/scope.h"

namespace thorin {

//#define THORIN_UNREACHABLE unreachable()
#define THORIN_UNREACHABLE assert(false && "Unreachable")
#define dlog(world,...) world.DLOG(__VA_ARGS__)
#define type_dump(world,name,d) world.DLOG("{} {} : {}",name,d,d->type())


size_t getDim(const Def* def) {
    // TODO: test def, idef, tuple
    if(auto arr=def->isa<Arr>()) {
        return arr->shape()->as<Lit>()->get<uint8_t>();
    }else if(auto arr=def->type()->isa<Arr>()) {
        return getDim(def->type());
    }else{
        dlog(def->world(),"  def {} : {}, dim {}",def,def->type(),def->num_projs());
        return def->num_projs();
        // ptr -> 1
        // tuple -> size
    }
}

bool isFatPtrType(World& world_,const Def* type) {
    if(auto sig=type->isa<Sigma>(); sig && sig->num_ops()==2) {
        // TODO: maybe use original type to detect

        //        isFatPtr = isa_sized_type(sig->op(0));
        dlog(world_,"  ty {}", type);
        dlog(world_,"  num ops {}", type->num_ops());
        dlog(world_,"  num projs {}", type->num_projs());
        dlog(world_,"  fst {}", type->op(0));
        //        dlog(world_,"  fst Test {}",isa<Tag::Int>(sig->op(0)));
        dlog(world_,"  snd {}", type->op(1));
        //        dlog(world_,"  snd Test {}", isa<Tag::Ptr>(sig->op(1)));
        if( auto ptr=isa<Tag::Ptr>(sig->op(1));ptr &&
                                               isa<Tag::Int>(sig->op(0))
            ) {
            auto [pointee, addr_space] = ptr->arg()->projs<2>();
            if(pointee->isa<Arr>())
                return true;
        }
    }
    return false;
}

DefArray flat_tuple(const DefArray& defs, bool preserveFatPtr=false) {
    // or use concat
    std::vector<const Def*> v;
    for( auto def : defs) {
        if(auto tup=def->isa<Tuple>()) {
            auto dim = tup->num_ops();
            for (size_t j = 0; j < dim; j++) {
                v.push_back(tup->op(j));
            }
        }else {
            v.push_back(def);
        }
    }
    return {v};
}

const Pi* isReturning(const Pi* pi){
    if (pi->is_cn() && pi->num_doms() > 0) {
        auto ret = pi->dom(pi->num_doms() - 1);
        if (auto ret_pi = ret->isa<Pi>(); ret_pi != nullptr && ret_pi->is_cn()) return ret_pi;
    }

    return nullptr;
}

DefArray vars_without_mem_cont(World& world, Lam* lam) {
    type_dump(world,"  get vars of",lam);
    dlog(world,"  has ret_var {}",lam->ret_var());
    return {
        lam->num_vars()-( isReturning(lam->type()) == nullptr ? 1 : 2),
        [&](auto i) {
          return lam->var(i+1);
        }
    };
}


// multidimensional addition of values
// needed for operation differentiation
// we only need a multidimensional addition

// TODO: Currently: sum takes mem, adds a and b and calls cont
// TODO: possible: sum := \lambda mem a b cont. cont(mem, a+b)
const Lam* vec_add(World& world, const Def* a, const Def* b, const Def* cont) {
    dlog(world,"add {}:{} + {}:{}",a,a->type(),b,b->type());
    type_dump(world,"add cont",cont);
    if(auto arr = a->type()->isa<Arr>(); arr && !(arr->shape()->isa<Lit>())) { THORIN_UNREACHABLE; }

    auto sum_pb = world.nom_lam(world.cn(world.type_mem()), world.dbg("sum_pb"));
    type_dump(world,"  pb (sum)",sum_pb);
    sum_pb->set_filter(world.lit_true());

    if (auto aptr = isa<Tag::Ptr>(a->type())) {
        auto [ty,addr_space] = aptr->arg()->projs<2>();

        auto mem=sum_pb->mem_var();

        auto [mem2,a_v] = world.op_load(mem,a)->projs<2>();
        auto [mem3,b_v] = world.op_load(mem2,b)->projs<2>();

        auto res_cont_type = world.cn_mem_flat(a_v->type());
        auto res_cont = world.nom_lam(res_cont_type,world.dbg("ptr_add_cont"));
        type_dump(world,"  result cont",res_cont);
        auto sum_cont = vec_add(world,a_v,b_v,res_cont);
        sum_pb->set_body(world.app(sum_cont, mem3));
        sum_pb->set_filter(true);
        type_dump(world,"  sum cont",sum_cont);

        auto rmem=res_cont->mem_var();
        auto s_v= world.tuple(vars_without_mem_cont(world,res_cont));
        type_dump(world,"  result sum",s_v);
        auto [rmem2, sum_ptr]=world.op_slot(ty,rmem,world.dbg("add_slot"))->projs<2>();
        type_dump(world,"  sum_ptr",sum_ptr);
        auto rmem3 = world.op_store(rmem2,sum_ptr,s_v);

        res_cont->set_body(world.app(
            cont,
            flat_tuple({
                rmem3,
                sum_ptr
            })
        ));
        res_cont->set_filter(true);

        return sum_pb;
    }


    if(isFatPtrType(world,a->type())){
        auto [size_a, arr_a] = a->projs<2>();
        auto [size_b, arr_b] = b->projs<2>();
        // size_b has to be size_a
        dlog(world," add fat pointer of size {} (={})",size_a,size_b);
        type_dump(world," arr_a indef",arr_a);
        type_dump(world," arr_b indef",arr_b);

        auto arr_size_nat = world.op_bitcast(world.type_nat(),size_a);
        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr_a->type())->arg()->projs<2>();
        auto arr_sized_ty=world.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();

        type_dump(world," alloc array type",arr_sized_ty);

        auto [mem2,arr_c_def]=world.op_alloc(arr_sized_ty,sum_pb->mem_var())->projs<2>();
        type_dump(world," arr_c def",arr_c_def);

        auto arr_c = world.op_bitcast(arr_a->type(),arr_c_def);
        type_dump(world," arr_c indef",arr_c);
//        THORIN_UNREACHABLE;

        // TODO: replace with for loop
        auto loop_head = world.nom_lam(world.cn_mem(world.type_int_width(64)),world.dbg("add_loop_head"));
        auto loop = world.nom_lam(world.cn(world.type_mem()),world.dbg("add_loop_body"));
        auto loop_end = world.nom_lam(world.cn(world.type_mem()),world.dbg("add_loop_exit"));

        type_dump(world," loop head",loop_head);
        type_dump(world," loop",loop);
        type_dump(world," loop end",loop_end);

        auto cond = world.op(ICmp::ul,loop_head->var(1),size_a);
        loop_head->branch(size_a,cond,loop,loop_end,loop_head->mem_var());

        auto idx=loop_head->var(1);
        type_dump(world," var i",idx);
        type_dump(world," 1",world.lit_int_width(64,1));
        auto inc=world.op(Wrap::add,world.lit_nat(0),world.lit_int_width(64,1),idx);
        type_dump(world," i+1",inc);

        // store into c
        auto a_p=world.op_lea(arr_a,idx,world.dbg("a_p"));
        auto b_p=world.op_lea(arr_b,idx,world.dbg("b_p"));
        auto c_p=world.op_lea(arr_c,idx,world.dbg("c_p"));
        type_dump(world," a_p",a_p);
        type_dump(world," b_p",b_p);
        type_dump(world," c_p",c_p);

        // add pointers using vec_add
        // lea c, store into c

        auto loop_mem = loop->mem_var();

        auto [lmem2,a_v] = world.op_load(loop_mem,a_p)->projs<2>();
        auto [lmem3,b_v] = world.op_load(lmem2,   b_p)->projs<2>();
        loop_mem=lmem3;
        type_dump(world," a_v",a_v);
        type_dump(world," b_v",b_v);


        // load values manually to allow for easy (and direct) storage into c
//        auto elem_res_cont_type = world.cn_mem(a_v->type());
        auto elem_res_cont_type = world.cn_mem_flat(a_v->type());
        auto elem_res_cont = world.nom_lam(elem_res_cont_type,world.dbg("tuple_add_cont"));
        auto element_sum_pb = vec_add(world,a_v,b_v,elem_res_cont);
        auto c_v = world.tuple(vars_without_mem_cont(world,elem_res_cont));
        type_dump(world," elem_res_cont",elem_res_cont);
        type_dump(world," elem_sum_pb",element_sum_pb);
        type_dump(world," c_v",c_v);

        auto res_mem=elem_res_cont->mem_var();
        res_mem=world.op_store(res_mem,c_p,c_v);

//        set loop
        loop->set_body(world.app(element_sum_pb, loop_mem));
        loop->set_filter(true);

        elem_res_cont->set_body(world.app(
            loop_head,
            {
                res_mem,
                inc
            }
        ));
        elem_res_cont->set_filter(true);

        loop_end->set_body(world.app(
            cont,
            flat_tuple({loop_end->mem_var(),
                        world.tuple({size_a,arr_c})
                       })
        ));
        loop_end->set_filter(true);


        sum_pb->set_body(world.app(
            loop_head,
            {
                mem2,
                world.lit_int_width(64,0)
            }
        ));
        sum_pb->set_filter(true);

        return sum_pb;
    }

    auto dim = getDim(a);
    auto dimb = getDim(b);
    assert(dim==dimb && "Dimension in add should be equal");

    if(dim==1){
        sum_pb->set_body(world.app(
            cont,
            flat_tuple({sum_pb->mem_var(),
                world.op(ROp::add,(nat_t)0,a,b)
            })
        ));
        sum_pb->set_filter(true);
        return sum_pb;
    }


    DefArray ops{dim};
    auto ret_cont_type = cont->type()->as<Pi>();
    auto current_cont=sum_pb;

    for (size_t i = 0; i < ops.size(); ++i) {
        // adds component-wise both vectors
        auto ai=world.extract(a,i); // use op?
        auto bi=world.extract(b,i);
        dlog(world,"  {}th: {}:{} + {}:{}",i,ai,ai->type(),bi,bi->type());
        auto res_cont_type = world.cn_mem_flat(ai->type());
        auto res_cont = world.nom_lam(res_cont_type,world.dbg("tuple_add_cont"));
        type_dump(world,"  result cont",res_cont);
        auto sum_call=vec_add(world,ai,bi,res_cont);
        ops[i]=world.tuple(vars_without_mem_cont(world,res_cont));

        current_cont->set_body(world.app(
            sum_call,
            current_cont->mem_var()
        ));
        current_cont->set_filter(true);

        current_cont=res_cont;
    }

    current_cont->set_body(world.app(
        cont,
        flat_tuple({current_cont->mem_var(), world.tuple(ops)})
    ));
    current_cont->set_filter(true);

    return sum_pb;

}


std::pair<const Def*,const Def*> lit_of_type(World& world, const Def* mem, const Def* type, const Def* like, r64 lit, const Def* dummy) {
    // TODO: a monad would be easier for memory
    dlog(world,"create literal of type {}",type);
    if(like){
        type_dump(world,"  like reference",like);
    }

    auto isFatPtr = isFatPtrType(world,type);
    if(isFatPtr) {
        type_dump(world,"  zero of fat ptr ty",type);
        assert(like!= nullptr);
        auto [arr_size,_] = like->projs<2>();

        auto ptr_ty = as<Tag::Ptr>(type->op(1));
        type_dump(world,"  ptr ty",ptr_ty);
        auto [arr_ty,addr_space] = ptr_ty->arg()->projs<2>();
        type_dump(world,"  arr ty",arr_ty);
        auto arr=arr_ty->as<Arr>();

        auto arr_size_nat = world.op_bitcast(world.type_nat(),arr_size);
        type_dump(world,"  arr size nat",arr_size_nat);
        auto arr_sized_ty=world.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();
        type_dump(world,"  arr_sized_ty",arr_sized_ty);

        auto [mem2,ptr_arr]=world.op_alloc(arr_sized_ty,mem)->projs<2>();
        type_dump(world,"ptr arr",ptr_arr);
        auto shape=arr_size_nat;//arr->shape();
        type_dump(world,"ptr arr shape",shape);
        auto body = arr->body();
        type_dump(world,"ptr arr body",body);
        auto [mem3, body_lit] = lit_of_type(world,mem2,body,nullptr,lit,dummy);
        type_dump(world,"ptr arr body lit",body_lit);
        auto init=world.pack(shape,body_lit);
        type_dump(world,"init pack",init); // trick for zero init
        auto mem4=world.op_store(mem3,ptr_arr,init);
        auto fat_ptr_arr = world.tuple({arr_size,ptr_arr});
        type_dump(world,"fat ptr arr",fat_ptr_arr);
        return {mem4,fat_ptr_arr};
    }

    // TODO: not for idef array
    if (auto ptr = isa<Tag::Ptr>(type)) {
        auto [ty,addr_space] = ptr->arg()->projs<2>();

        // ty->isa<Arr> handled already by isFatPtr
        if(auto arr=ty->isa<Arr>()) {
            auto [mem2,ptr_arr]=world.op_alloc(ty,mem)->projs<2>();
            auto shape=arr->shape();
            type_dump(world,"ptr arr shape",shape);
            auto body = arr->body();
            type_dump(world,"ptr arr body",body);
            auto [mem3, body_lit] = lit_of_type(world,mem2,body,nullptr,lit,dummy);
            type_dump(world,"ptr arr body lit",body_lit);
            auto init=world.pack(shape,body_lit);
            type_dump(world,"init pack",init); // trick for zero init
            auto mem4=world.op_store(mem3,ptr_arr,init);
            type_dump(world,"ptr arr",ptr_arr);
            return {mem4,ptr_arr};
        }

        auto [mem2, lit_ptr]=world.op_slot(ty,mem,world.dbg("lit_slot"))->projs<2>();
        auto [mem3, lit_res] = lit_of_type(world,mem2,ty,like,lit,dummy);
        auto mem4 = world.op_store(mem3,lit_ptr,lit_res);

        return {mem4,lit_ptr};
    }
    const Def* litdef;
    if (auto real = isa<Tag::Real>(type))
        litdef= world.lit_real(as_lit(real->arg()), lit);
    else if (auto a = type->isa<Arr>()) {
        auto dim = a->shape()->as<Lit>()->get<uint8_t>();
        dlog(world,"create array literal of dim {}",dim);
        DefArray ops{dim};
        for (size_t i = 0; i < dim; ++i) {
            auto [nmem, op]=lit_of_type(world,mem,a->body(),like,lit,dummy);
            mem=nmem;
            ops[i]=op;
        }
        litdef= world.tuple(ops);
    }else if(auto sig = type->isa<Sigma>()) {
        std::vector<const Def*> zops;
        dlog(world,"create tuple (Sigma) literal of dim {}",sig->num_ops());
        int idx=0;
        for (auto op : sig->ops()) {
            auto [nmem, zop]=lit_of_type(world,mem,op,like->proj(idx),lit,dummy);
            mem=nmem;
            zops.push_back(zop);
            idx++;
        }
        litdef= world.tuple(zops);
    }
    else litdef= dummy;

    return {mem,litdef};
}

std::pair<const Def*,const Def*> ONE(World& world, const Def* mem, const Def* def, const Def* like, const Def* dummy) { return lit_of_type(world, mem, def, like, 1, dummy); }
std::pair<const Def*,const Def*> ZERO(World& world, const Def* mem, const Def* def, const Def* like, const Def* dummy) { return lit_of_type(world, mem, def, like, 0, dummy); }
std::pair<const Def*,const Def*> ZERO(World& world, const Def* mem, const Def* def, const Def* like) { return ZERO(world,mem, def, like, nullptr);}
std::pair<const Def*,const Def*> ONE(World& world, const Def* mem, const Def* def, const Def* like) { return ONE(world,mem, def, like, nullptr);}
std::pair<const Def*,const Def*> ZERO(World& world, const Def* mem, const Def* def) { return ZERO(world,mem, def, nullptr);}
std::pair<const Def*,const Def*> ONE(World& world, const Def* mem, const Def* def) { return ONE(world,mem, def, nullptr);}


std::pair<const Def*,const Def*> oneHot(World& world_, const Def* mem,u64 idx, const Def* shape, const Def* like, const Def* s) {
    auto [rmem, v] = ZERO(world_,mem,shape,like,s);
    return {rmem,world_.insert_unsafe(v,idx,s)};
}

std::pair<const Def*,const Def*> oneHot(World& world_, const Def* mem, const Def* idx, const Def* shape, const Def* like, const Def* s) {
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
        return oneHot(world_,mem,*lit,shape,like,s);
    }else {
        // TODO: wrong
        // TODO: fix like
        dlog(world_, "non-lit oh");
        auto dim = getDim(shape);
        dlog(world_,"dim: {}",dim);

        // instead of
        // ((1,0,0),(0,1,0),(0,0,1)) # idx
        // we build
        // ((1,0,0)#idx, (0,1,0)#idx, (0,0,1)#idx)
        // which is equivalent
        // but allows flattening (toplevel tupel)
        DefArray ohv{dim};

        for (size_t i = 0; i < dim; ++i) {
            dlog(world_,"  shape {}: {}",i,shape);
            // correct type shape here? => probably not but we know that the tranpose is the same
            auto [nmem, oh]=oneHot(world_,mem,i,shape,like,s);
            dlog(world_,"  oh {}: {}",i,oh);
            mem=nmem;
            ohv[i]=world_.extract_unsafe(oh,idx);
        }

        auto oh=world_.tuple(ohv);
        type_dump(world_,"oh",oh);
        return {mem,oh};
    }
}

namespace {

class AutoDiffer {
public:
    AutoDiffer(World& world, const Def2Def& src_to_dst, const Def* A_)
        : world_{world}
        , src_to_dst_{src_to_dst}
        , A_src{A_}
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

        dlog(world_,"  A: {}", A_);
        dlog(world_,"  tangent type of A: {}", A);
        dlog(world_,"Finished Construction");
    }

    const Def* reverse_diff(Lam* src); // top level function to compute the reverse differentiation of a function
private:
    const Def* j_wrap(const Def* def); // 'identity' (except for lambdas, functions, and applications) traversal annotating the pullbacks
    const Def* j_wrap_rop(ROp op, const Def* a, const Def* b); // pullback computation for predefined functions, specifically operations like +, -, *, /
    void derive_external( const Lam* fun, Lam* pb, Lam* fw, Lam* res_lam);
    void derive_numeric( const Lam* fun, Lam* lam_d, const Def* x, r64 delta );

    const Def* zero_pb(const Def* type, const Def* dbg);
    const Def* j_wrap_tuple(DefArray tuple);

    const Def* seen(const Def* src); // lookup in the map

    // chains cn[:mem, A, cn[:mem, B]] and cn[:mem, B, cn[:mem, C]] to a toplevel cn[:mem, A, cn[:mem, C]]
    const Def* chain(const Def* a, const Def* b);
    const Pi* createPbType(const Def* A, const Def* B);
    const Def* extract_pb(const Def* j_extract, const Def* tuple);

    World& world_;
    Def2Def src_to_dst_; // mapping old def to new def
    DefMap<const Def*> pullbacks_;  // <- maps a *copied* src term (a dst term) to its pullback function
    DefMap<const Def*> pointer_map;
    DefMap<const Def*> structure_map;
    const Def* A, *A_src, *zero_grad;// input type

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


const Def* AutoDiffer::j_wrap_tuple(DefArray tuple) {
    // the pullback of a tuple is tuple of pullbacks for each component
    // we need to distinguish [mem, r32] from <<2::nat,r32>>
    // a tuple with memory as argument is used in applications but we only want the pullback of the "real" arguments
//    type_dump(world_,"tuple",tuple);
    auto tuple_dim=tuple.size();
    dlog(world_,"  num of ops: {}",tuple_dim);
    // jwrap each component
    DefArray ops{tuple_dim, [&](auto i) { return j_wrap(tuple[i]); }};
    dlog(world_,"  jwrapped elements: {, }",ops);

    auto isMemTuple = tuple_dim>0 && isa<Tag::Mem>(tuple[0]->type());
    auto isRetTuple = isMemTuple && tuple_dim>1 && tuple[tuple_dim-1]->type()->isa<Pi>();

    if(isMemTuple) {
        ops[0] = j_wrap(tuple[0]);
    }

    // reconstruct the tuple term
    auto dst = world_.tuple(ops);
    dlog(world_,"  tuple: {,}",tuple);
    type_dump(world_,"  jwrapped tuple:",dst);

    // a bit of partial eval, peephole
    if(isMemTuple &&
        (tuple_dim==2 ||
         (tuple_dim==3 && isRetTuple))) {
        pullbacks_[dst]=pullbacks_[ops[1]];
        return dst;
    }
    dlog(world_,"tangent type of dst: {} => {}",dst->type(),world_.tangent_type(dst->type(),false));
    dlog(world_,"tuple dim: {}",tuple_dim);

    // TODO: simplify
    // TODO: could a more modular approach with more primitive pullbacks make this code easier?

    // get pullbacks for each component w.r. to A
    // apply them with the component of the scalar from the tuple pullback
    // sum them up

    size_t real_arg_num;
    if(isRetTuple)
        real_arg_num=tuple_dim-2;
    else if(isMemTuple)
        real_arg_num=tuple_dim-1;
    else
        real_arg_num=tuple_dim;

//    const Def* trimmed_ty;
//    auto tuple_ty = tuple->type();
    auto trimmed_var_ty=DefArray(real_arg_num,
    [&] (auto i) {
        return tuple[isMemTuple ? i+1 : i]->type();
    });

    auto trimmed_ty=world_.sigma(trimmed_var_ty);


    auto pi = createPbType(A,trimmed_ty);
    auto pb = world_.nom_lam(pi, world_.dbg("tuple_pb"));
    dlog(world_,"  complete tuple pb type: {}",pi);
    pb->set_filter(world_.lit_true());

    type_dump(world_,"  A:",A);
    auto pbT = pi->as<Pi>()->doms().back()->as<Pi>();
    dlog(world_,"  intermediate tuple pb type: {}",pbT);
    dlog(world_,"  should be cn_mem of {}",A);

    auto current_sum_pb = world_.nom_lam(pbT, world_.dbg("tuple_sum_pb"));
    current_sum_pb->set_filter(world_.lit_true());
    type_dump(world_,"  sum 0 pb {}",current_sum_pb);

    pb->set_body(world_.app(
        current_sum_pb,
        flat_tuple({
            pb->mem_var(),
            zero_grad
        }) ));

    auto tuple_of_pb = world_.tuple(
        DefArray{real_arg_num, [&](auto i) { return pullbacks_[isMemTuple ? ops[i+1] : ops[i]]; }}
    );

    /**
     * pb = \lambda mem scalars ret. sum_pb_0 (mem,0)
     * sum_pb_i = \lambda mem sum_i. pb_i (mem, s_i, res_pb_i)
     * res_pb_i = \lambda mem res_i. sum_cont (mem, sum_i, res_i, sum_pb_{i+1})
     * sum_pb_n = \lambda mem sum. ret (mem, sum)
     */

    dlog(world_,"  tuple size of pbs: {}",real_arg_num);
    for (size_t i = 0; i < real_arg_num; ++i) {

        const Def* op;
        if(isMemTuple) {
            op=ops[i+1];
        }else {
            op=ops[i];
        }
        auto op_pb=pullbacks_[op];
        dlog(world_,"    build pb sum op {}: {} : {}",i,op,op->type());
        dlog(world_,"      pb {}",op_pb);
        dlog(world_,"      pb {} : {}",op_pb,op_pb->type());
        auto scalar = pb->var(i+1, world_.dbg("s"));
        dlog(world_,"      pb var: {}:{}",
             scalar,
             scalar->type());

        auto res_pb = world_.nom_lam(pbT, world_.dbg("res_pb"));
        res_pb->set_filter(world_.lit_true());
        type_dump(world_,"  result pb {}",res_pb);

        current_sum_pb->set_body(world_.app(
            op_pb,
            flat_tuple( {
                current_sum_pb->mem_var(),
                scalar,
                res_pb
        })));

        auto next_current_sum_pb = world_.nom_lam(pbT, world_.dbg("tuple_sum_pb"));
        next_current_sum_pb->set_filter(world_.lit_true());

        auto sum_cont_pb = vec_add(world_,
                                   world_.tuple(vars_without_mem_cont(world_,current_sum_pb)),
                                   world_.tuple(vars_without_mem_cont(world_,res_pb)),
                                   next_current_sum_pb);
        type_dump(world_,"  sum_cont {}",sum_cont_pb);
        res_pb->set_body(world_.app(
            sum_cont_pb,
            res_pb->mem_var()
        ));

        current_sum_pb=next_current_sum_pb;
    }
    current_sum_pb->set_body(world_.app(
        pb->ret_var(),
        current_sum_pb->var()));

    // TODO: multiple arguments

    dlog(world_,"  tuple pbs {}",pb);
    pullbacks_[dst]=pb;
    type_dump(world_,"  pullback for tuple",pullbacks_[dst]);
    return dst;
}


const Def* AutoDiffer::chain(const Def* a, const Def* b) {
    // chaining of two pullbacks is composition due to the
    // nature of a pullback as linear map => application corresponds to (matrix-)multiplication

    // res = b(a(x))
    // a : A -> B
    // b : B -> C
    // res : A -> C

    auto at = a->type()->as<Pi>();
    auto bt = b->type()->as<Pi>();
    type_dump(world_,"   chain fun a",a);
    type_dump(world_,"   chain fun b",b);

    auto A = world_.params_without_return_continuation(at);
    auto B = world_.params_without_return_continuation(bt);
    auto C = world_.sigma(bt->doms().back()->as<Pi>()->doms().skip_front());
    auto B2 = world_.sigma(at->doms().back()->as<Pi>()->doms().skip_front());
    dlog(world_,"   A {}",A);
    dlog(world_,"   B {}",B);
    dlog(world_,"   C {}",C);
    dlog(world_,"   B2 {}",B2);

    auto pi = world_.cn_mem_ret_flat(A, C);
    auto toplevel = world_.nom_lam(pi, world_.dbg("chain"));

    auto middlepi = world_.cn_mem_flat(B);
    auto middle = world_.nom_lam(middlepi, world_.dbg("chain_2"));

    toplevel->set_body(world_.app(a, flat_tuple({toplevel->mem_var(), world_.tuple(vars_without_mem_cont(world_,toplevel)), middle})));
    middle->set_body(world_.app(b, flat_tuple({middle->mem_var(), world_.tuple(vars_without_mem_cont(world_,middle)), toplevel->ret_var()})));

    toplevel->set_filter(world_.lit_true());
    middle->set_filter(world_.lit_true());

    return toplevel;
}

// pullback for a function of type A->B => pb of B result regarding A
const Pi* AutoDiffer::createPbType(const Def* A, const Def* B) {
    // one could keep A "normal" and use tangent type here and at the uses to create a pb ZERO,
//    return world_.cn_mem_ret(world_.tangent_type(B,false), A);
    return world_.cn_mem_ret_flat(world_.tangent_type(B, false), A);
}


//const Def* AutoDiffer::extract_pb(const Def* j_tuple, const Def* j_idx) {

// tuple for artificial tuple (fat_ptr)
// TODO: pb of [mem,[i64,ptr]] (fat_ptr) is cn[mem, i64,ptr,cn[...]]
const Def* AutoDiffer::extract_pb(const Def* j_extract, const Def* tuple) {
    if(pullbacks_.count(j_extract))
        return pullbacks_[j_extract];
    auto extract = j_extract->as<Extract>();

    auto extract_type=extract->type();

    auto isFatPtr=isFatPtrType(world_,extract_type);

    auto tangent_type =
        isFatPtr ?
        extract_type->op(1) :
        extract_type;

    auto pi = createPbType(A, tangent_type);
    auto pb = world_.nom_lam(pi, world_.dbg("extract_pb"));
    pb->set_filter(world_.lit_true());
    type_dump(world_,"  pb of extract: ",pb);
    type_dump(world_,"  extract: ",extract);

    const Def* idx=extract->index();
    type_dump(world_,"  extract of tup: ",tuple);
    type_dump(world_,"  idx: ",idx);
    auto tuple_ty = tuple->type();
    auto tuple_pb = pullbacks_[tuple];

    dlog(world_,"  pb of tuple: {}",tuple_pb);
    dlog(world_,"  type pb of tuple: {}",tuple_pb->type());

    DefArray pb_args;

    // is tuple & index
    // TODO: integrate into OH
    if(auto lit = idx->isa<Lit>()) {
        // would save from tuples
        // but can not occur as partial evaluation removes such projections

        dlog(world_,"  extract pb for lit index");
        auto isMemTuple=isa<Tag::Mem>(tuple->type()->proj(0));
        auto pb_domain=tuple_pb->type()->as<Pi>()->dom();//as<Sigma>();
        dlog(world_,"  pb domain: {}",pb_domain);

        int index_lit = lit->get<uint8_t>();

        // TODO: one hot vector, mem tuple
        auto dim=pb_domain->num_ops();
        DefArray args{dim};
        auto mem=pb->mem_var();
        for (size_t i = 0; i < dim; ++i) {
            if(i==0)
                args[i]=mem;
            else if(i==dim-1) {
                args[i]=pb->ret_var();
            } else if(i==index_lit) {
                args[i]= world_.tuple(vars_without_mem_cont(world_,pb));
            }else {
                // TODO: correct index
                auto [nmem, v]=ZERO(world_,mem,pb_domain->op(i), tuple->proj(i));
                mem=nmem;
                args[i]=v;
            }
        }
        args[0]=mem;
        pb_args=args;

    }else {
        dlog(world_,"  non lit index");

        auto [rmem, ohv] = oneHot(world_,pb->mem_var(), idx,world_.tangent_type(tuple_ty,false),nullptr,pb->var(1,world_.dbg("s")));
        pb_args=
            flat_tuple({
                rmem,
                ohv,
                pb->ret_var()
            });
    }

    dlog(world_,"    pb {}",pb);
    dlog(world_,"    pb ty {}",pb->type());
    dlog(world_,"    tuple_pb {}",tuple_pb);
    dlog(world_,"    tuple_pb ty {}",tuple_pb->type());
    dlog(world_,"    pb_args {, }",pb_args);
    type_dump(world_,"    pb_args tuple ",world_.tuple(pb_args));
    type_dump(world_,"    pb_args flat tuple ",world_.tuple(flat_tuple(pb_args)));

    pb->set_body(world_.app(
        tuple_pb,
        pb_args
        ));
    return pb;
}


// loads pb from shadow slot, updates pb for the ptr, returns, mem and pb for the loaded value
std::pair<const Def*,const Def*> AutoDiffer::reloadPtrPb(const Def* mem, const Def* ptr, const Def* dbg, bool generateLoadPb) {
    type_dump(world_,"  reload for ptr",ptr);
    dlog(world_,"  shadow ptr {}",pointer_map[ptr]);
    type_dump(world_,"  shadow ptr",pointer_map[ptr]);
    auto [pb_load_mem,pb_load_fun] = world_.op_load(mem,pointer_map[ptr],dbg)->projs<2>();
    pullbacks_[ptr]=pb_load_fun;
    return {pb_load_mem,pb_load_fun};
}


// top level entry point after creating the AutoDiffer object
// a mapping of source arguments to dst arguments is expected in src_to_dst
const Def* AutoDiffer::reverse_diff(Lam* src) {
    // For each param, create an appropriate pullback. It is just the (one-hot) identity function for each of those.
    type_dump(world_,"Apply RevDiff to src",src);

    auto dst_lam = src_to_dst_[src]->as_nom<Lam>();
    current_mem=dst_lam->mem_var();

    auto src_var = src->var();
    auto dst_var = src_to_dst_[src_var];
    type_dump(world_,"src variable",src_var);
    type_dump(world_,"dst variable",dst_var);

    auto var_sigma = src_var->type()->as<Sigma>();

    auto size = var_sigma->num_ops() - 2;
    DefArray trimmed_var_ty(size);
    for (size_t i = 0; i < size; ++i) {
        trimmed_var_ty[i] = var_sigma->op(i+1);
    }
    auto trimmed_var_sigma = world_.sigma(trimmed_var_ty);
    dlog(world_,"trimmed var sigma: {}", trimmed_var_sigma); // A?

    auto idpi = createPbType(A,trimmed_var_sigma);
    auto idpb = world_.nom_lam(idpi, world_.dbg("param_id"));

    type_dump(world_,"idpb",idpb);

    auto real_params = DefArray(
        dst_lam->num_vars()-2,
        [&](auto i) {
          return dst_lam->var(i+1);
        });

    type_dump(world_," create zero grad for",A);
    type_dump(world_,"   reference",world_.tuple(real_params));
    auto [current_mem_,zero_grad_] = ZERO(world_,current_mem,A,world_.tuple(real_params));
    current_mem=current_mem_;
    zero_grad=zero_grad_;
    type_dump(world_,"zero_grad",zero_grad);

    dlog(world_,"Set IDPB");

    // ret only resp. non-mem, non-cont
    auto args = DefArray(
        src->num_vars()-1,
        [&](auto i) {
          if(i==0)
              return idpb->mem_var();
          return idpb->var(i);
        });
    idpb->set_body(world_.app(idpb->ret_var(), args));
    idpb->set_filter(world_.lit_true());


    type_dump(world_,"idpb body",idpb->body());

    pullbacks_[dst_var] = idpb;


    type_dump(world_,"init arg",dst_var);
        for(size_t i = 0, e = src->num_vars(); i < e; ++i) {
            auto dvar = dst_lam->var(i);
            dlog(world_," var {}: {} : {}",i,dvar,dvar->type());
            if(dvar == dst_lam->ret_var() || dvar == dst_lam->mem_var()) {
                continue;
            }
            // solve the problem of inital array pb in extract pb
            pullbacks_[dvar]= extract_pb(dvar, dst_lam->var());
            type_dump(world_," pb",pullbacks_[dvar]);
            initArg(dvar);
        }

    dlog(world_,"Initialization finished, start jwrapping");
    dlog(world_,"  tangent type of A: {}", A);
    // translate the body => get correct applications of variables using pullbacks
    auto dst = j_wrap(src->body());
    return dst;
}

void AutoDiffer::initArg(const Def* dst) {
    // TODO: iterate (recursively) over tuple
    // create shadow slots for pointers

    auto arg_ty = dst->type();
    dlog(world_,"Arg of Type: {}", arg_ty);


    // we need to initialize the shadow ptr slot for
    // ptr args here instead of at store & load (first usage)
    // as the slot needs the correct pullback (from the ptr object)
    // to be stored and loaded
    // when the ptr shadow slot is accessed it has to have the correct
    // content in the current memory object used to load
    // this is only possible at a common point before all usages
    //   => creation / first mentioning
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
        type_dump(world_, "Pb of var", pullbacks_[dst]);

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
    // TODO: like
    auto [mem2, half_delta_lit] = lit_of_type(world_, lam_d->mem_var(), type, nullptr, delta/2, nullptr);
    auto [mem3, delta_lit] = lit_of_type(world_, lam_d->mem_var(), type, nullptr,delta, nullptr);

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
        // TODO:
        auto [mem2, two] = lit_of_type(world_,pb->mem_var(), real_type, nullptr,2.0,nullptr);
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

const Def* AutoDiffer::zero_pb(const Def* type, const Def* dbg) {
    auto zeropi = createPbType(A,type);
    dlog(world_,"  zero_pi ty: {}",zeropi);
    auto zeropb = world_.nom_lam(zeropi, world_.dbg(dbg));
    type_dump(world_,"  pb (zero)",zeropb);
    zeropb->set_filter(world_.lit_true());

    auto rmem=zeropb->mem_var();
    auto zero = zero_grad;//ZERO(world_,zeropb->mem_var(), A);

    type_dump(world_," zero:",zero);

    // TODO: inline in ZERO?
    DefArray args= flat_tuple({rmem,zero});
    zeropb->set_body(world_.app(zeropb->ret_var(), args));
    return zeropb;
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
        type_dump(world_,"replacement:",dst);
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

        if( isReturning(lam->type())) {
            auto dst = world_.op_rev_diff(lam);
            type_dump(world_,"  new lam",dst);

            // should not be needed => TODO: handle higher order pb correctly in app
            pullbacks_[dst]=zero_pb(lam->type(),world_.dbg("zero_pb_lam"));
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
        // never executed but needed for tuple pb
        dlog(world_,"  compute pb ty of lam: {}",lam->type());
        pullbacks_[dst] = zero_pb(lam->type(),world_.dbg("zero_pb_lam2"));



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
            pullbacks_[a]= extract_pb(a,ab);
            type_dump(world_,"  created pb for a",pullbacks_[a]);
            pullbacks_[b]= extract_pb(b,ab);
            type_dump(world_,"  created pb for b",pullbacks_[b]);
        }
        auto dst = j_wrap_rop(ROp(rop.flags()), a, b);
        src_to_dst_[rop] = dst;
        type_dump(world_,"  result of rop app",dst);
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

        auto isFatPtr = isFatPtrType(world_,args->type());

        // avoid case distinction
        // copy the bitcast but exchange the arguments with the new ones
        const Def* dst, *dst_pb_org_ty, *arg_pb_ty;
        if(isFatPtr) {
            auto [size,arr] = args->projs<2>();
            type_dump(world_,"  array from args:",arr);
            auto dst_arr=world_.app(cast->callee(),arr);
            dst_pb_org_ty=dst_arr->type();
            dst = world_.tuple({size,dst_arr});
            arg_pb_ty = arr->type();
        }else {
            dst = world_.app(cast->callee(),args);
            dst_pb_org_ty=dst->type();
            arg_pb_ty = args->type();
        }
        type_dump(world_,"  Wraped Bitcast:",dst);
        // mostly a zero pb that does not need to be recomputed
        // but for arrays we have to bitcast the argument in opposite direction

        auto arg_pb = pullbacks_[args];
        type_dump(world_,"  arg ty:",args->type());
        type_dump(world_,"  arg pb:",arg_pb);

        auto pb_ty = createPbType(A,dst_pb_org_ty);
        type_dump(world_,"  pb_ty",pb_ty);

        auto pb = world_.nom_lam(pb_ty, world_.dbg("pb_bitcast"));
        pb->set_filter(world_.lit_true());

        type_dump(world_,"  pb_var 1",pb->var(1));
        type_dump(world_,"  pb_var 2",pb->var(2));
        auto cast_arg = world_.op_bitcast(arg_pb_ty,pb->var(2));
        type_dump(world_,"  cast pb_var",cast_arg);

        pb->set_body( world_.app(arg_pb,
         flat_tuple({
            pb->mem_var(),
            world_.tuple({pb->var(1), cast_arg}),
            pb->ret_var()
         }) ));

        pullbacks_[dst]=pb;
        type_dump(world_,"  set pb:",pullbacks_[dst]);

//        THORIN_UNREACHABLE;
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

        auto dst_alloc = world_.op_alloc(type,mem_arg,alloc->dbg());
        auto [r_mem,arr] = dst_alloc->projs<2>();
        type_dump(world_,"  orig alloc",alloc);
        type_dump(world_,"  dst alloc",dst_alloc);
        type_dump(world_,"  arr",arr);

        type_dump(world_,"  inner type",type);
        auto size=type->as<Arr>()->shape();
        auto int_size=world_.op_bitcast(world_.type_int_width(64),size);
        dlog(world_,"  allocation size {}",size);
        dlog(world_,"  allocation int size {}",int_size);
        auto dst_fat_ptr=world_.tuple({int_size,arr});
        auto dst=world_.tuple({r_mem,dst_fat_ptr});
        type_dump(world_,"  dst fat ptr",dst_fat_ptr);
        type_dump(world_,"  dst",dst);

        current_mem = r_mem;
        src_to_dst_[alloc] = dst;

        // no shadow needed
        // TODO: shadow if one handles alloc like a ptr (for definite)
        auto pb = zero_pb(ptr_type,world_.dbg("pb_alloc"));

        type_dump(world_,"  alloc pb",pb);
        pullbacks_[arr] = pb;
        pullbacks_[dst_fat_ptr]=pullbacks_[arr];
        pullbacks_[dst]=pullbacks_[arr]; // for call f(rmem, arr)
        pullbacks_[dst_alloc]=pullbacks_[arr]; // for mem extract
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
        dlog(world_,"  args: {,}",lea->args());
        dlog(world_,"  type: {}",lea->type());
        type_dump(world_,"  lea",lea);
        dlog(world_,"  callee type: {}",lea->callee_type());
        auto ptr_ty = as<Tag::Ptr>(lea->type());
        auto [ty,addr_space] = ptr_ty->arg()->projs<2>();
        dlog(world_,"  inner type: {}", ty);

        auto fat_ptr=j_wrap(lea->arg(0));
        type_dump(world_,"  lea orig arg:", lea->arg(0));
        type_dump(world_,"  lea fat_ptr:", fat_ptr);
        auto [arr_size,arr] = fat_ptr->projs<2>();
        type_dump(world_,"  lea arr:", arr);
        auto idx = j_wrap(lea->arg(1)); // not necessary
        type_dump(world_,"  dst idx:", idx);
        auto dst = world_.op_lea(arr,idx);
        type_dump(world_,"  dst lea:", dst);

        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr->type())->arg()->projs<2>();

        type_dump(world_,"  ty: ",ty);
        type_dump(world_,"  arr_ty: ",arr_ty);
        dlog(world_,"  arr_ty_node_name: {}",arr_ty->node_name());
        auto pi = createPbType(A,ty);
        type_dump(world_,"  lea pi: ",pi);
        auto pb = world_.nom_lam(pi, world_.dbg("pb_lea"));
        pb->set_filter(world_.lit_true());
        type_dump(world_,"  lea pb: ",pb);

        type_dump(world_,"  arr size",arr_size);
        auto arr_size_nat = world_.op_bitcast(world_.type_nat(),arr_size);
        type_dump(world_,"  arr size nat",arr_size_nat);
        auto arr_sized_ty=world_.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();
        type_dump(world_,"  arr_sized_ty",arr_sized_ty);
        auto ptr_arr_sized_ty = world_.type_ptr(arr_sized_ty);
        type_dump(world_,"  ptr_arr_sized_ty",ptr_arr_sized_ty);
        // TODO: merge with ZERO?

        auto [mem2,ptr_arr]=world_.op_alloc(arr_sized_ty,pb->mem_var())->projs<2>();
        auto shape=arr_sized_ty->shape();
        type_dump(world_,"ptr arr shape",shape);
        auto body = arr_sized_ty->body();
        type_dump(world_,"ptr arr body",body);
        auto [mem3, body_lit] = ZERO(world_,mem2,body);
        type_dump(world_,"ptr arr body lit",body_lit);
        auto init=world_.pack(shape,body_lit);
        type_dump(world_,"init pack",init); // trick for zero init
        auto mem4=world_.op_store(mem3,ptr_arr,init);
        type_dump(world_,"ptr arr",ptr_arr);

        assert(pullbacks_.count(fat_ptr) && "arr from lea should already have an pullback");

        type_dump(world_,"  fat_ptr pb",pullbacks_[fat_ptr]);
        auto ptr_arr_idef = pullbacks_[fat_ptr]->type()->as<Pi>()->dom(2);
        dlog(world_,"  pullback arr arg: {}", ptr_arr_idef);
        auto ptr_arr_arg = world_.op_bitcast(ptr_arr_idef,ptr_arr);
        type_dump(world_,"  ptr_arr casted:",ptr_arr_arg);
        auto fat_ptr_arr_arg = world_.tuple({arr_size,ptr_arr_arg});
        type_dump(world_,"  ptr_arr fat_ptr:",fat_ptr_arr_arg);

        auto scal_ptr = world_.op_lea(ptr_arr_arg,idx);
        auto v = pb->var(1);
        auto mem5 = world_.op_store(mem4,scal_ptr,v);
        type_dump(world_,"  ptr_arr",ptr_arr);
        type_dump(world_,"  ptr_arr_arg",ptr_arr_arg);


        dlog(world_,"  pullback of arr (or rather its fat_ptr): {}",pullbacks_[fat_ptr]);
        dlog(world_,"  of type: {}",pullbacks_[fat_ptr]->type());

        type_dump(world_,"  lea pb type:",pb);

        pb->set_body( world_.app(
            pullbacks_[fat_ptr],
            flat_tuple({
                mem5,
                fat_ptr_arr_arg,
                pb->ret_var()
            })
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

        src_to_dst_[lea]=dst;

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
                    // to prevent error in load for tuple pb
                    auto [nmem,pb_loaded]=reloadPtrPb(dst_mem,dst_ptr,world_.dbg("ptr_slot_pb_loadL"),true);
                    dst_mem=nmem;
                    pullbacks_[dst]=pb_loaded;

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

        if (isReturning(callee->type()->as<Pi>())) {
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

                lam->set_debug_name(cal_lam->name() + "_diff_impl");
                lam2->set_debug_name(lam->name() + "_cont");
                gradlam->set_debug_name(cal_lam->name() + "_pb");
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


            type_dump(world_,"  wrapped args: ",d_arg);
            auto m = d_arg->proj(0);
            auto num_projs = d_arg->num_projs();
            auto ret_arg = d_arg->proj(num_projs-1);
            auto args=DefArray(
                num_projs-2,
                [&](auto i) {
                  return d_arg->proj(i+1);
                });
            auto arg= world_.tuple(args);
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

            auto ret_pb = chained->var(chained->num_vars() - 1);
            type_dump(world_,"  ret var pb",ret_pb);
            auto chain_pb = chain(ret_pb,arg_pb);
            type_dump(world_,"  chain pb",chain_pb);

            // TODO
            chained->set_body( world_.app(
                ret_arg,
                flat_tuple({
                    chained->mem_var(),
                    world_.tuple(vars_without_mem_cont(world_,chained)),
                    chain_pb
                })
                ));
            chained->set_filter(world_.lit_true());
            type_dump(world_,"  build chained (app pb) ",chained);

            // TODO ?
            auto dst = world_.app(dst_callee, flat_tuple({m,arg,chained}));

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
            dlog(world_,"  is arg in pb: {}",pullbacks_.count(d_arg));
            if(pullbacks_.count(d_arg)) {
                dlog(world_,"  arg pb: {}",pullbacks_[d_arg]);
                type_dump(world_,"  arg pb: ",pullbacks_[d_arg]);
            }
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
                    DefArray(
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
        auto tuple_dim=getDim(tuple->type());
        DefArray ops{tuple_dim, [&](auto i) { return tuple->proj(i); }};
        auto dst = j_wrap_tuple(ops);
        src_to_dst_[tuple] = dst;
        return dst;
    }

    if (auto pack = def->isa<Pack>()) {
        // no pullback for pack needed
        type_dump(world_,"Pack",pack);

        auto dim = as_lit(pack->type()->arity());
        auto tup=DefArray(
            dim,
            [&](auto i) {
              return pack->body();
            });
        dlog(world_,"  pack to tuple {,}",tup);
        auto dst= j_wrap_tuple(tup);
        type_dump(world_,"  jwrapped pack",dst);
        src_to_dst_[pack] = dst;
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

        auto dst = world_.extract_unsafe(jtup, jeidx,extract->dbg());
        type_dump(world_,"  jwrapped extract",dst);
        src_to_dst_[extract] = dst;
        if(isa<Tag::Mem>(dst->type())) {
            dlog(world_,"  extract is mem => no pb");
        }else{
            pullbacks_[dst] = extract_pb(dst,jtup);
            type_dump(world_,"  pullback of extract",pullbacks_[dst]);
        }
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
        pullbacks_[lit] = zero_pb(lit->type(), world_.dbg("zero_pb_lit"));
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
    const Def* dst;
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
            dst = world_.op(ROp::add, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "+"));

            pb->set_body(world_.app(apb, {pb->mem_var(), pb->var(1), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), pb->var(1), end}));
            break;
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
            dst = world_.op(ROp::sub, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "-"));

            pb->set_body(world_.app(apb, {pb->mem_var(), pb->var(1), middle}));
            auto [rmem,one] = ONE(world_,middle->mem_var(), o_type);
            middle->set_body(world_.app(bpb, {rmem, world_.op(ROp::mul, (nat_t)0, pb->var(1), world_.op_rminus((nat_t)0, one)), end}));
            // all args 1..n as tuple => vector for addition
            break;

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
            dst = world_.op(ROp::mul, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "*"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), b), middle}));
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op(ROp::mul, (nat_t)0, pb->var(1), a), end}));
            break;

        }
            // ∇(a / b) = λz. (g* (z * h) - h* (z * g))/h²
        case ROp::div: {
            //    a*(1/b * z)          => a*(z/b)
            //  + b*(a * -b^(-2) * z)  => b*(-z*a/(b*b))
            dst = world_.op(ROp::div, (nat_t)0, a, b);
            pb->set_dbg(world_.dbg(pb->name() + "/"));

            pb->set_body(world_.app(apb, {pb->mem_var(), world_.op(ROp::div, (nat_t)0, pb->var(1), b), middle}));
            auto za=world_.op(ROp::mul, (nat_t)0, pb->var(1), a);
            auto bsq=world_.op(ROp::mul, (nat_t)0, b, b);
            middle->set_body(world_.app(bpb, {middle->mem_var(), world_.op_rminus((nat_t)0, world_.op(ROp::div, (nat_t)0, za, bsq)), end}));
            break;
        }
        default:
            // only +, -, *, / are implemented as basic operations
            THORIN_UNREACHABLE;
    }

    auto adiff = world_.tuple(vars_without_mem_cont(world_,middle));
    auto bdiff = world_.tuple(vars_without_mem_cont(world_,end));
    auto sum_pb=vec_add(world_,adiff,bdiff,pb->ret_var());
    end->set_body(world_.app(sum_pb, end->mem_var()));
    pullbacks_[dst] = pb;
    return dst;
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

        type_dump(app->world()," arg",app->arg());
        auto isClosure = app->num_args()>1;

        auto fun_arg = isClosure ? app->arg(1) : app->arg(0);
        type_dump(app->world()," fun arg",fun_arg);

        auto src_lam = fun_arg->as_nom<Lam>();
        auto src_pi = src_lam->type();
        // function to differentiate
        // this should be something like `cn[:mem, r32, cn[:mem, r32]]`
        auto& world = src_lam->world();

        // We get for `A -> B` the type `A -> (B * (B -> A))`.
        //  i.e. cn[:mem, A, [:mem, B]] ---> cn[:mem, A, cn[:mem, B, cn[:mem, B, cn[:mem, A]]]]
        //  take input, return result and return a function (pullback) taking z and returning the derivative
        const Pi* dst_pi;
        type_dump(world,"app type",app->type());
        if(isClosure)
            dst_pi = app->type()->op(1)->as<Pi>();
        else
            dst_pi = app->type()->as<Pi>(); // multi dim as array
        auto dst_lam = world.nom_lam(dst_pi, world.dbg("top_level_rev_diff_" + src_lam->name()));
        dst_lam->set_filter(src_lam->filter()); // copy the unfold filter
        // use src to not dilute tangent transformation with left type transformation (only matters for arrays)
        auto A = world.params_without_return_continuation(src_pi); // input variable(s) => possible a pi type (array)

        // is cn[mem, B0, ..., Bm, pb] => skip mem and pb
        auto B = world.params_without_return_continuation(dst_pi->dom()->ops().back()->as<Pi>());
        dlog(world,"AD of function from {} to {}",A,B);
        type_dump(world,"Transform:",src_lam);
        type_dump(world,"Result:",dst_lam);

        // The actual AD, i.e. construct "sq_cpy"
        Def2Def src_to_dst;
        // src_to_dst maps old definitions to new ones
        // here we map the arguments of the lambda

        src_to_dst[src_lam] = dst_lam;
        src_to_dst[src_lam->var()] = dst_lam->var();
        auto differ = AutoDiffer{world, src_to_dst, A};
        dst_lam->set_body(differ.reverse_diff(src_lam));

        auto dst=isClosure ? world.insert(app->arg(),1,dst_lam) : dst_lam;

        type_dump(world,"dst: ",dst);

        return dst;
    }}}
    return def;
}

}