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
        return def->num_projs();
        // ptr -> 1
        // tuple -> size
    }
}

bool isFatPtrType(World& world_,const Def* type) {
    if(auto sig=type->isa<Sigma>(); sig && sig->num_ops()==2) {
        // TODO: maybe use original type to detect

        //        isFatPtr = isa_sized_type(sig->op(0));
        //        
        //        
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
        if(auto tup=def->type()->isa<Sigma>()) {
            auto dim = def->num_projs();
            for (size_t j = 0; j < dim; j++) {
                v.push_back(def->proj(j)); }
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

DefArray vars_without_mem_cont(Lam* lam) {
    // ? 1 : 0 is superfluous (see 7.8.4 in C++ 20 standard) but increases readability
    return lam->vars().skip(1, isReturning(lam->type()) != nullptr ? 1 : 0);
}
// multidimensional addition of values
// needed for operation differentiation
// we only need a multidimensional addition

const Lam* repeatLam(World& world, const Def* count, const Lam* body){
    auto loop_entry = world.nom_filter_lam(world.cn({world.type_mem(), world.cn(world.type_mem())}),world.dbg("loop_entry"));
    auto loop_head = world.nom_lam(world.cn_mem(world.type_int_width(64)),world.dbg("loop_head"));
    auto loop = world.nom_filter_lam(world.cn(world.type_mem()),world.dbg("loop"));
    auto loop_end = world.nom_filter_lam(world.cn(world.type_mem()),world.dbg("loop_exit"));
    auto loop_continue = world.nom_filter_lam(world.cn(world.type_mem()),world.dbg("loop_continue"));
    auto cond = world.op(ICmp::ul,loop_head->var(1),count);

    loop_entry->set_body(world.app(loop_head, {loop_entry->mem_var(), world.lit_int_width(64,0)}));

    loop_head->branch(world.lit_false(),cond,loop,loop_end,loop_head->mem_var());

    auto idx = loop_head->var(1);
    auto inc = world.op(Wrap::add,world.lit_nat(0),world.lit_int_width(64,1),idx);

    loop->set_body(world.app(body, {loop->mem_var(), idx, loop_continue}));

    loop_continue->set_body(world.app( loop_head, { loop_continue->mem_var(), inc } ));
    loop_end->set_body(world.app( loop_entry->ret_var(), loop_end->mem_var() ));

    return loop_entry;
}

std::pair<const Lam*, Lam*> repeatLam(World& world, const Def* count){
    Lam* body = world.nom_filter_lam(world.cn({world.type_mem(), world.type_int_width(64), world.cn(world.type_mem())}), world.dbg("loop_body"));
    const Lam* loop = repeatLam(world, count, body);
    return {loop, body};
}

// TODO: Currently: sum takes mem, adds a and b and calls cont
// TODO: possible: sum := \lambda mem a b cont. cont(mem, a+b)
const Lam* vec_add(World& world, const Def* a, const Def* b, const Def* cont) {
    if(auto arr = a->type()->isa<Arr>(); arr && !(arr->shape()->isa<Lit>())) { THORIN_UNREACHABLE; }

    auto sum_pb = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("sum_pb"));
    if (auto aptr = isa<Tag::Ptr>(a->type())) {
        auto [ty,addr_space] = aptr->arg()->projs<2>();

        auto mem=sum_pb->mem_var();

        auto [mem2,a_v] = world.op_load(mem,a)->projs<2>();
        auto [mem3,b_v] = world.op_load(mem2,b)->projs<2>();

        auto res_cont_type = world.cn_mem_flat(a_v->type());
        auto res_cont = world.nom_filter_lam(res_cont_type,world.dbg("ptr_add_cont"));
        auto sum_cont = vec_add(world,a_v,b_v,res_cont);
        sum_pb->set_body(world.app(sum_cont, mem3));
        auto rmem=res_cont->mem_var();
        auto s_v= world.tuple(vars_without_mem_cont(res_cont));
        auto [rmem2, sum_ptr]=world.op_slot(ty,rmem,world.dbg("add_slot"))->projs<2>();
        auto rmem3 = world.op_store(rmem2,sum_ptr,s_v);

        res_cont->set_body(world.app(
            cont,
            flat_tuple({
                rmem3,
                sum_ptr
            })
        ));

        return sum_pb;
    }

    if(isFatPtrType(world,a->type())){
        auto [size_a, arr_a] = a->projs<2>();
        auto [size_b, arr_b] = b->projs<2>();
        // size_b has to be size_a
        auto arr_size_nat = world.op_bitcast(world.type_nat(),size_a);
        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr_a->type())->arg()->projs<2>();
        auto arr_sized_ty=world.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();
        auto [mem2,arr_c_def]=world.op_alloc(arr_sized_ty,sum_pb->mem_var())->projs<2>();
        auto arr_c = world.op_bitcast(arr_a->type(),arr_c_def);
//        THORIN_UNREACHABLE;

        auto [loop, loop_body] = repeatLam(world, size_a);

        auto loop_mem = loop_body->mem_var();
        auto idx = loop_body->var(1);
        auto continue_loop = loop_body->ret_var();

        // TODO: replace with for loop
        // store into c
        auto a_p=world.op_lea(arr_a,idx,world.dbg("a_p"));
        auto b_p=world.op_lea(arr_b,idx,world.dbg("b_p"));
        auto c_p=world.op_lea(arr_c,idx,world.dbg("c_p"));
        // add pointers using vec_add
        // lea c, store into c

        auto [left_load_mem,a_v] = world.op_load(loop_mem,a_p)->projs<2>();
        auto [right_load_mem,b_v] = world.op_load(left_load_mem,   b_p)->projs<2>();
        // load values manually to allow for easy (and direct) storage into c
        auto elem_res_cont_type = world.cn_mem_flat(a_v->type());
        auto elem_res_cont = world.nom_filter_lam(elem_res_cont_type,world.dbg("tuple_add_cont"));
        auto element_sum_pb = vec_add(world,a_v,b_v,elem_res_cont);
        auto c_v = world.tuple(vars_without_mem_cont(elem_res_cont));
        auto res_mem=elem_res_cont->mem_var();
        auto store_addition = res_mem=world.op_store(res_mem,c_p,c_v);

        auto end = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("sum_pb"));
        end->set_body(world.app(
            cont,
            flat_tuple({
                end->mem_var(),
                world.tuple({size_a,arr_a})
            })
        ));

//        set loop
        loop_body->set_body(world.app(element_sum_pb, right_load_mem));
        elem_res_cont->set_body(world.app(continue_loop, store_addition));
        sum_pb->set_body(world.app(loop, {sum_pb->mem_var(), end}));

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
        return sum_pb;
    }

    DefArray ops{dim};
    auto ret_cont_type = cont->type()->as<Pi>();
    auto current_cont=sum_pb;

    for (size_t i = 0; i < ops.size(); ++i) {
        // adds component-wise both vectors
        auto ai=world.extract(a,i); // use op?
        auto bi=world.extract(b,i);
        auto res_cont_type = world.cn_mem_flat(ai->type());
        auto res_cont = world.nom_filter_lam(res_cont_type,world.dbg("tuple_add_cont"));
        auto sum_call=vec_add(world,ai,bi,res_cont);
        ops[i]=world.tuple(vars_without_mem_cont(res_cont));

        current_cont->set_body(world.app(
            sum_call,
            current_cont->mem_var()
        ));

        current_cont=res_cont;
    }

    current_cont->set_body(world.app(
        cont,
        flat_tuple({current_cont->mem_var(), world.tuple(ops)})
    ));

    return sum_pb;

}

const Def* copy(World& world, const Def* inputArr, const Def* outputArr, const Def* size){
    auto [loop, loop_body] = repeatLam(world, size);

    auto idx = loop_body->var(1);

    auto input_p = world.op_lea(inputArr,idx,world.dbg("a_p"));
    auto output_p = world.op_lea(outputArr,idx,world.dbg("stencil_p"));

    auto loop_mem = loop_body->mem_var();

    auto [load_mem, loadedValue] = world.op_load(loop_mem, input_p )->projs<2>();
    auto storeMem = world.op_store(load_mem, output_p, loadedValue );

    loop_body->set_body(world.app(loop_body->ret_var(), storeMem));

    return loop;
}

class Flow{
    Lam* lam_ = nullptr;
    Lam* init_;
    const Def* mem_;
    World& world_;
    u32 length = 0;
public:
    Flow(World& world) : world_(world){
        init_ = world_.nom_filter_lam(world_.cn(world_.type_mem()), world_.dbg("flow_init_11"));
        assign(init_);
    }

    Flow(World& world, Lam* init) : world_(world){
        init_ = init;
        assign(init_);
    }

    void assign(Lam* lam){
        assert(lam_ == nullptr || lam_->is_set());
        assert(!lam->body());
        lam_ = lam;
        mem_ = lam->mem_var();
    }

    void runAfter(const Lam* enter, Lam* leave){
        lam_->set_body(world_.app(enter, mem_));
        assign(leave);
    }

    const Lam* runAfter(const Lam* enter){
        return runAfter(enter, mem_);
    }

    const Lam* runAfter(const Lam* enter, const Def* mem){
        assert(lam_);
        length++;
        auto callback = world_.nom_filter_lam(world_.cn(world_.type_mem()), world_.dbg("flow_init"));
        if(auto lam = enter->doms().back()->isa<Pi>()){
            lam_->set_body(world_.app(enter, {mem, callback}));
        }else{
            lam_->set_body(world_.app(enter, mem));
        }

        assign(callback);
        return callback;
    }

    void finish(const Def* enter, Defs args = {}){
        length++;
        auto arguments = world_.tuple(flat_tuple({mem_, world_.tuple(args)}));
        lam_->set_body(world_.app(enter, arguments));
        lam_ = nullptr;
    }

    const Lam* getInit(){
        return init_;
    }
};

const Def* derive_numeric_walk(World& world, const Def* ref, const Def* h, const Lam* f, const Def* fx, const Def* s, Flow& flow) {
    auto fun_result_pi = f->doms().back()->as<Pi>();

    if (auto ptr = isa<Tag::Ptr>(ref->type())) {
        auto [ty,addr_space] = ptr->arg()->projs<2>();

        auto offset_param = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("offset_param"));

        //save value for restore later
        auto [save_mem,save] = world.op_load( offset_param->mem_var(), ref )->projs<2>();
        auto returnLam = flow.runAfter(offset_param);
        offset_param->set_body(world.app(returnLam, save_mem));

        auto masked_f = world.nom_filter_lam(world.cn({world.type_mem(), save->type(), fun_result_pi}), world.dbg("offset_param"));

        //change value at ptr location to *p + h
        auto store_mem = world.op_store(masked_f->mem_var(), ref, masked_f->var(1));

        //restore value at ptr location back to original value
        auto restoreLam = world.nom_filter_lam(fun_result_pi, world.dbg("clean_up"));
        auto retored_mem = world.op_store(restoreLam->mem_var(), ref, save);

        restoreLam->set_body(world.app(masked_f->ret_var(), {retored_mem, restoreLam->var(1)}));
        masked_f->set_body(world.app(f, {store_mem, ref, restoreLam}));

        return derive_numeric_walk(world, save, h, masked_f, fx, s, flow);
    }

    if(isFatPtrType(world,ref->type())){
        auto [size_a, arr_ref] = ref->projs<2>();

        //allocate array for resulting gradients
        auto alloc_gradients = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("alloc_gradients"));

        auto arr_size_nat = world.op_bitcast(world.type_nat(), size_a);
        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr_ref->type())->arg()->projs<2>();
        auto arr_sized_ty = world.arr(arr_size_nat, arr_ty->as<Arr>()->body())->as<Arr>();
        auto [gradient_mem,gradient_arr] = world.op_alloc(arr_sized_ty, alloc_gradients->mem_var())->projs<2>();
        gradient_arr = world.op_bitcast(arr_ref->type(), gradient_arr);

        const Lam* returnLam = flow.runAfter(alloc_gradients);
        alloc_gradients->set_body(world.app(returnLam, gradient_mem));

        auto [loop, loop_body] = repeatLam(world, size_a);

        flow.runAfter(loop);

        auto loop_mem = loop_body->mem_var();
        auto idx = loop_body->var(1);
        auto continue_loop = loop_body->ret_var();

        auto ref_p = world.op_lea(arr_ref,idx,world.dbg("ref_p"));

        auto masked_f = world.nom_filter_lam(world.cn({world.type_mem(), ref_p->type(), fun_result_pi}), world.dbg("masked_f"));
        masked_f->set_body(world.app(f, {masked_f->mem_var(), ref, masked_f->ret_var()}));

        Flow loopFlow{world, loop_body};
        auto result = derive_numeric_walk(world, ref_p, h, masked_f, fx, s, loopFlow);
        auto continue_loop_lam = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("continue_loop_lam"));

        auto lea_gradient = world.op_lea(gradient_arr, idx);
        auto store_gradient_mem = world.op_store(continue_loop_lam->mem_var(), lea_gradient, result);

        continue_loop_lam->set_body(world.app(continue_loop, store_gradient_mem));

        loopFlow.finish(continue_loop_lam);

        return world.tuple({size_a, gradient_arr});
    }

    auto dim = getDim(ref);

    if(dim==1){
        if (isa<Tag::Int>(ref->type())) {
            return world.lit_real(64, 0.0);
        }else{
            auto f_call = world.nom_filter_lam(world.cn(world.type_mem()), world.dbg("f_call"));

            auto quotient = world.nom_filter_lam(fun_result_pi, world.dbg("quotient"));
            auto result = world.nom_filter_lam(fun_result_pi, world.dbg("result"));

            //call function with value offset
            f_call->set_body(world.app(
                f,
                {
                    f_call->mem_var(),
                    world.op(ROp::add, (nat_t)0, ref, h),
                    quotient
                }
            ));

            //differential quotient
            auto gradient = world.op(ROp::mul, (nat_t) 0,
                 world.op(ROp::div, (nat_t) 0,
                          world.op(Conv::r2r,
                                   ref->type(),
                                   world.op(ROp::sub, (nat_t) 0, quotient->var(1), fx)
                          ),
                          h
                 ),
                 s
            );

            quotient->set_body(world.app(result, {quotient->mem_var(), gradient}));
            flow.runAfter(f_call, result);
            return result->var(1);
        }
    }

    DefArray tuple_result{dim};

    for (size_t i = 0; i < dim; ++i) {
        // adds component-wise both vectors
         // use op?
        auto current = world.extract(ref, i);

        DefArray ops{dim + 2};
        auto masked_f = world.nom_filter_lam(world.cn({world.type_mem(), current->type(), fun_result_pi}), world.dbg("masked_f"));

        for( size_t j = 0; j < dim; ++j  ){
            if(j != i){
                ops[j + 1] = world.extract(ref, j);
            }
        }

        ops[0] = masked_f->mem_var();
        ops[i + 1] = masked_f->var(1);
        ops[dim + 1] = masked_f->ret_var();

        masked_f->set_body(world.app(f, ops));

        tuple_result[i] = derive_numeric_walk(world, current, h, masked_f, fx, s, flow);
    }

    return world.tuple(tuple_result);
}

std::pair<const Def*,const Def*> lit_of_type(World& world, const Def* mem, const Def* type, const Def* like, r64 lit, const Def* dummy) {
    // TODO: a monad would be easier for memory
    if(like){
    }

    auto isFatPtr = isFatPtrType(world,type);
    if(isFatPtr) {
        assert(like!= nullptr);
        auto [arr_size,_] = like->projs<2>();

        auto ptr_ty = as<Tag::Ptr>(type->op(1));
        auto [arr_ty,addr_space] = ptr_ty->arg()->projs<2>();
        auto arr=arr_ty->as<Arr>();

        auto arr_size_nat = world.op_bitcast(world.type_nat(),arr_size);
        auto arr_sized_ty=world.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();
        auto [mem2,ptr_arr]=world.op_alloc(arr_sized_ty,mem)->projs<2>();
        auto shape=arr_size_nat;
        auto body = arr->body();
        auto [mem3, body_lit] = lit_of_type(world,mem2,body,nullptr,lit,dummy);
        auto init=world.pack(shape,body_lit);
        auto mem4=world.op_store(mem3,ptr_arr,init);
        auto fat_ptr_arr = world.tuple({arr_size,ptr_arr});
        return {mem4,fat_ptr_arr};
    }

    // TODO: not for idef array
    if (auto ptr = isa<Tag::Ptr>(type)) {
        auto [ty,addr_space] = ptr->arg()->projs<2>();

        // ty->isa<Arr> handled already by isFatPtr
        if(auto arr=ty->isa<Arr>()) {
            auto [mem2,ptr_arr]=world.op_alloc(ty,mem)->projs<2>();
            auto shape=arr->shape();
            auto body = arr->body();
            auto [mem3, body_lit] = lit_of_type(world,mem2,body,nullptr,lit,dummy);
            auto init=world.pack(shape,body_lit);
            auto mem4=world.op_store(mem3,ptr_arr,init);
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
        DefArray ops{dim, [&](auto){
            auto [nmem, op] = lit_of_type(world,mem,a->body(),like,lit,dummy);
            mem=nmem;
            return op;
        }};
        litdef= world.tuple(ops);
    }else if(auto sig = type->isa<Sigma>()) {
        auto zops = sig->ops().map([&](auto op, auto index){
            auto [nmem, zop]=lit_of_type(world,mem,op,like->proj(index),lit,dummy);
            mem=nmem;
            return zop;
        });

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

    if(auto lit = isa_lit(idx)) {
        return oneHot(world_,mem,*lit,shape,like,s);
    }else {
        // TODO: wrong
        // TODO: fix like
        auto dim = getDim(shape);
        // instead of
        // ((1,0,0),(0,1,0),(0,0,1)) # idx
        // we build
        // ((1,0,0)#idx, (0,1,0)#idx, (0,0,1)#idx)
        // which is equivalent
        // but allows flattening (toplevel tupel)
        DefArray ohv{dim};

        for (size_t i = 0; i < dim; ++i) {
            // correct type shape here? => probably not but we know that the tranpose is the same
            auto [nmem, oh]=oneHot(world_,mem,i,shape,like,s);
            mem=nmem;
            ohv[i]=world_.extract_unsafe(oh,idx);
        }

        auto oh=world_.tuple(ohv);
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
    }

    const Def* reverse_diff(Lam* src); // top level function to compute the reverse differentiation of a function
private:
    const Def* j_wrap(const Def* def); // 'identity' (except for lambdas, functions, and applications) traversal annotating the pullbacks
    const Def* j_wrap_convert(const Def* def);
    const Def* j_wrap_rop(ROp op, const Def* a, const Def* b); // pullback computation for predefined functions, specifically operations like +, -, *, /
    void derive_external( const Lam* fun, Lam* pb, Lam* fw, Lam* res_lam);
    void derive_numeric(const Lam *fun, Lam *source, const Def *target, Lam *fw, const Def* fx, const Def* s, r32 delta);

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
//    
    auto tuple_dim=tuple.size();
    // jwrap each component
    DefArray ops{tuple_dim, [&](auto i) { return j_wrap(tuple[i]); }};
    auto isMemTuple = tuple_dim>0 && isa<Tag::Mem>(tuple[0]->type());
    auto isRetTuple = isMemTuple && tuple_dim>1 && tuple[tuple_dim-1]->type()->isa<Pi>();

    if(isMemTuple) {
        ops[0] = j_wrap(tuple[0]);
    }

    // reconstruct the tuple term
    auto dst = world_.tuple(ops);
    // a bit of partial eval, peephole
    if(isMemTuple &&
        (tuple_dim==2 ||
         (tuple_dim==3 && isRetTuple))) {
        pullbacks_[dst]=pullbacks_[ops[1]];
        return dst;
    }
    // TODO: simplify
    // TODO: could a more modular approach with more primitive pullbacks make this code easier?

    // get pullbacks for each component w.r. to A
    // apply them with the component of the scalar from the tuple pullback
    // sum them up

    auto trimmed_tuple = tuple.skip(isMemTuple, isRetTuple);
    auto trimed_ops = ops.skip(isMemTuple, isRetTuple);

    auto trimmed_ty=world_.sigma(
        trimmed_tuple.map( [] (auto* def, auto) { return def->type(); } )
    );
    auto pi = createPbType(A,trimmed_ty);
    auto pb = world_.nom_filter_lam(pi, world_.dbg("tuple_pb"));
    auto pbT = pi->as<Pi>()->doms().back()->as<Pi>();
    auto current_sum_pb = world_.nom_filter_lam(pbT, world_.dbg("tuple_sum_pb"));
    pb->set_body(world_.app(
        current_sum_pb,
        flat_tuple({
            pb->mem_var(),
            zero_grad
        })
    ));

    /**
     * pb = \lambda mem scalars ret. sum_pb_0 (mem,0)
     * sum_pb_i = \lambda mem sum_i. pb_i (mem, s_i, res_pb_i)
     * res_pb_i = \lambda mem res_i. sum_cont (mem, sum_i, res_i, sum_pb_{i+1})
     * sum_pb_n = \lambda mem sum. ret (mem, sum)
     */
    for (size_t i = 0; i < trimed_ops.size(); ++i) {
        const Def* op = trimed_ops[i];
        auto op_pb = pullbacks_[op];
        auto scalar = pb->var(i+1, world_.dbg("s"));

        auto res_pb = world_.nom_filter_lam(pbT, world_.dbg("res_pb"));
        current_sum_pb->set_body(world_.app(
            op_pb,
            flat_tuple( {
                current_sum_pb->mem_var(),
                scalar,
                res_pb
            })
        ));

        auto next_current_sum_pb = world_.nom_filter_lam(pbT, world_.dbg("tuple_sum_pb"));

        auto sum_cont_pb = vec_add(world_,
                                   world_.tuple(vars_without_mem_cont(current_sum_pb)),
                                   world_.tuple(vars_without_mem_cont(res_pb)),
                                   next_current_sum_pb);
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
    pullbacks_[dst]=pb;
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
    auto A = world_.params_without_return_continuation(at);
    auto B = world_.params_without_return_continuation(bt);
    auto C = world_.sigma(bt->doms().back()->as<Pi>()->doms().skip_front());
    auto B2 = world_.sigma(at->doms().back()->as<Pi>()->doms().skip_front());
    auto pi = world_.cn_mem_ret_flat(A, C);
    auto toplevel = world_.nom_filter_lam(pi, world_.dbg("chain"));

    auto middlepi = world_.cn_mem_flat(B);
    auto middle = world_.nom_filter_lam(middlepi, world_.dbg("chain_2"));

    toplevel->set_body(world_.app(a, flat_tuple({toplevel->mem_var(), world_.tuple(vars_without_mem_cont(toplevel)), middle})));
    middle->set_body(world_.app(b, flat_tuple({middle->mem_var(), world_.tuple(vars_without_mem_cont(middle)), toplevel->ret_var()})));

    return toplevel;
}

// pullback for a function of type A->B => pb of B result regarding A
const Pi* AutoDiffer::createPbType(const Def* A, const Def* B) {
    // one could keep A "normal" and use tangent type here and at the uses to create a pb ZERO,
//    return world_.cn_mem_ret(world_.tangent_type(B,false), A);
    auto BT = world_.tangent_type(B,false);
    auto flatten_dom=true;
    auto flatten_codom=true;
//    if(isa<Tag::Ptr>(B)) { // for nonflat fat_ptr
//        flatten_dom=false;
//    }
    auto pb_ty= world_.cn_mem_ret_flat(BT, A, {}, flatten_dom, flatten_codom);
    dlog(world_,"pb_ty {}", pb_ty);
    dlog(world_,"  tangent B {}", BT);
    return pb_ty;
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
    auto pb = world_.nom_filter_lam(pi, world_.dbg("extract_pb"));
    dlog(world_,"extract pb {} : {}", pb, pb->type());
    const Def* idx=extract->index();
    auto tuple_ty = tuple->type();
    auto tuple_pb = pullbacks_[tuple];
    DefArray pb_args;

    // is tuple & index
    // TODO: integrate into OH
    if(auto lit = idx->isa<Lit>()) {
        // would save from tuples
        // but can not occur as partial evaluation removes such projections
        auto isMemTuple=isa<Tag::Mem>(tuple->type()->proj(0));
        auto pb_domain=tuple_pb->type()->as<Pi>()->dom();//as<Sigma>();
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
                args[i]= world_.tuple(vars_without_mem_cont(pb));
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
        auto [rmem, ohv] = oneHot(world_,pb->mem_var(), idx,world_.tangent_type(tuple_ty,false),nullptr,pb->var(1,world_.dbg("s")));
        pb_args=
            flat_tuple({
                rmem,
                ohv,
                pb->ret_var()
            });
    }
    pb->set_body(world_.app(
        tuple_pb,
        pb_args
    ));
    return pb;
}
// loads pb from shadow slot, updates pb for the ptr, returns, mem and pb for the loaded value
std::pair<const Def*,const Def*> AutoDiffer::reloadPtrPb(const Def* mem, const Def* ptr, const Def* dbg, bool generateLoadPb) {
    auto [pb_load_mem,pb_load_fun] = world_.op_load(mem,pointer_map[ptr],dbg)->projs<2>();
    pullbacks_[ptr]=pb_load_fun;
    return {pb_load_mem,pb_load_fun};
}
// top level entry point after creating the AutoDiffer object
// a mapping of source arguments to dst arguments is expected in src_to_dst
const Def* AutoDiffer::reverse_diff(Lam* src) {
    // For each param, create an appropriate pullback. It is just the (one-hot) identity function for each of those.
    auto dst_lam = src_to_dst_[src]->as_nom<Lam>();
    current_mem=dst_lam->mem_var();

    auto src_var = src->var();
    auto dst_var = src_to_dst_[src_var];
    auto var_sigma = src_var->type()->as<Sigma>();

    DefArray trimmed_var_ty = var_sigma->ops().skip(1,1);
    auto trimmed_var_sigma = world_.sigma(trimmed_var_ty);
    auto idpi = createPbType(A,trimmed_var_sigma);
    auto idpb = world_.nom_filter_lam(idpi, world_.dbg("param_id"));
    auto vars = dst_lam->vars();
    auto real_params = vars.skip(1,1);
    auto [current_mem_,zero_grad_] = ZERO(world_,current_mem,A,world_.tuple(real_params));
    current_mem=current_mem_;
    zero_grad=zero_grad_;
    // ret only resp. non-mem, non-cont
    auto idpb_vars = idpb->vars();
    auto args = idpb_vars.skip_back();
    idpb->set_body(world_.app(idpb->ret_var(), args));
    pullbacks_[dst_var] = idpb;
    auto src_vars = src->vars();
    for(auto dvar : src_vars.skip(1,1)) {
        // solve the problem of inital array pb in extract pb
        pullbacks_[dvar]= extract_pb(dvar, dst_lam->var());
        initArg(dvar);
    }
    // translate the body => get correct applications of variables using pullbacks
    auto dst = j_wrap(src->body());
    return dst;
}

void AutoDiffer::initArg(const Def* dst) {
    // TODO: iterate (recursively) over tuple
    // create shadow slots for pointersq

    auto arg_ty = dst->type();
    // we need to initialize the shadow ptr slot for
    // ptr args here instead of at store & load (first usage)
    // as the slot needs the correct pullback (from the ptr object)
    // to be stored and loaded
    // when the ptr shadow slot is accessed it has to have the correct
    // content in the current memory object used to load
    // this is only possible at a common point before all usages
    //   => creation / first mentioning
    if(auto ptr= isa<Tag::Ptr>(arg_ty)) {
        auto ty = ptr->arg()->projs<2>()[0];
        auto dst_mem = current_mem;
        auto [pb_mem, pb_ptr] = ptrSlot(arg_ty, dst_mem)->projs<2>();
        pointer_map[dst] = pb_ptr;
        // write the pb into the slot
        auto pb_store_mem = world_.op_store(pb_mem, pb_ptr, pullbacks_[dst], world_.dbg("pb_arg_id_store"));
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


void AutoDiffer::derive_numeric(const Lam *fun, Lam *source, const Def *target, Lam *fw, const Def* fx, const Def* s, r32 delta) {
    // https://www.overleaf.com/read/gdpfxvzqpfjf
    // # Numeric differentiation    for general case

    // d/dx f(x) ≈ (f(x+h/2)-f(x-h/2))/h     (local tangent)
    // or more efficient in multidim: (f(x+h)-f(x))/h

    auto x = world_.tuple(vars_without_mem_cont(fw));

    Flow flow{world_, source};
    auto h = world_.lit_real(64, delta);

    const Def* result = derive_numeric_walk(world_, x, h, fun, fx, s, flow);

    flow.finish(target, {result});
}


// fills in the body of pb (below called gradlam) which stands for f* the pullback function
// the pullback function takes a tangent scalar and returns the derivative
// fun is the original called external function (like exp, sin, ...) : A->B
// pb is the pullback B->A that might use the argument of fw in its computation
// fw is the new toplevel called function that invokes fun and hands over control to res_lam
// res_lam is a helper function that takes the result f(x) as argument and returns the result together with the pullback
void AutoDiffer::derive_external(const Lam *fun, Lam *pb, Lam *fw, Lam *res_lam) {
    std::string name = fun->name();
    // d/dx f(g(x)) = g'(x) f'(g(x))
    // => times s at front

    // x
//    const Def *fun_arg = fw->var(1);
    const Def *fun_arg = world_.tuple(vars_without_mem_cont(fw));
    // f(x)
    const Def *res = world_.tuple(vars_without_mem_cont(res_lam));
    // s (in an isolated environment s=1 -> f*(s) = df/dx)
    const Def *scal = world_.tuple(vars_without_mem_cont(pb));

    auto diff_name = name + "_diff";
    const Def* user_defined_diff = world_.lookup(diff_name);
    dlog(world_,"look for function {}",diff_name);


    dlog(world_,"externals: ");
    for( auto x : world_.externals() ){
        dlog(world_,x.first.c_str());
    }

    dlog(world_,"sea: ");
    auto sea=world_.defs();
    for( auto x : sea ){
        if(diff_name == x->name()){
//            dlog(world_,x->name().c_str());
            user_defined_diff = x;
            break;
        }
        if(x->name().find(diff_name) != std::string::npos){
            dlog(world_,x->name().c_str());
        }
//        if(x->isa<Lam>()) {
//            dlog(world_, x->name().c_str());
//        }
    }



    // wrapper to add times s around it
    auto return_type = pb->ret_var()->type()->as<Pi>();
    auto return_pi = return_type->op(0);

    auto returnCont = pb->ret_var();

    if (user_defined_diff != nullptr) {
        auto scal_mul_wrap = world_.nom_filter_lam(return_type, world_.dbg("scal_mul"));

        scal_mul_wrap->set_body(
                world_.app(
                        pb->ret_var(),
                        scal_mul_wrap->vars().map([&](auto var, size_t i) {
                            if (i == 0) {
                                return var;
                            } else {
                                return world_.op(ROp::mul, (nat_t) 0,
                                                 world_.op_bitcast(var->type(), scal),
                                                 var
                                );
                            }
                        })
                )
        );

        type_dump(world_,"found user diffed function",user_defined_diff);
        pb->set_body(world_.app(user_defined_diff, flat_tuple({pb->mem_var(), fun_arg, scal_mul_wrap})));
    } else if (name == "log") {
        const Def *log_d = world_.app(pb->ret_var(), {
                pb->mem_var(),
                world_.op(ROp::div, (nat_t) 0, scal, fun_arg)
        });

        pb->set_body(log_d);
    } else if (name == "exp") {
        // d exp(x)/d y = d/dy x * exp(x)
        pb->set_body(
                world_.app(pb->ret_var(),
                           {pb->mem_var(),
                            world_.op(ROp::mul, (nat_t) 0, res, scal)
                           }));
    } else if (name == "sqrt") {
        // d/dx g(sqrt(f(x))) = g'(sqrt(f(x))) * 1/(2sqrt(f(x))) * f'(x)
        // => sqrt(x) |-> lambda s. s/(2res) with res = sqrt(x)
        const Def *real_type = scal->type();
        // TODO:
        auto[mem2, two] = lit_of_type(world_, pb->mem_var(), real_type, nullptr, 2.0, nullptr);
        const Def *log_d = world_.app(returnCont, {mem2,
                      world_.op(ROp::div, (nat_t) 0,
                                scal,
                                world_.op(ROp::mul, (nat_t) 0, two, res)
                      )
        });

        pb->set_body(log_d);
    } else if (name == "sin") {
        // sin(x) |-> (sin(x), lambda s. s*cos(x))
        auto cos = world_.lookup("cos");

        if (cos == nullptr) {
            dlog(world_, "Error: no cos implementation found");
            THORIN_UNREACHABLE;
        }

        auto fun_return_type = fun->doms().back()->as<Pi>();
        auto fun_result = world_.nom_filter_lam(fun_return_type, world_.dbg("negate"));

        fun_result->set_body(world_.app(returnCont, {
            fun_result->mem_var(),
            world_.op(ROp::mul, (nat_t) 0, fun_result->var(1), scal)
        }));

        pb->set_body(world_.app(cos, {pb->mem_var(), fun_arg, fun_result}));
    } else if (name == "cos") {
        // lambda s. -s * sin(x)
        Lam *sin = (Lam *) world_.lookup("sin");

        if (sin == nullptr) {
            dlog(world_, "Error: no sin implementation found");
            THORIN_UNREACHABLE;
        }

        auto fun_return_type = fun->doms().back()->as<Pi>();
        auto negate = world_.nom_filter_lam(fun_return_type, world_.dbg("negate"));

        // -s * return of cos
        negate->set_body(world_.app(returnCont, {
            sin->mem_var(),
            world_.op(ROp::mul, (nat_t) 0, negate->var(1), world_.op_rminus((nat_t) 0, scal))
        }));

        pb->set_body(world_.app(sin, {pb->mem_var(), fun_arg, negate}));
    } else {
        derive_numeric(fun, pb, returnCont, fw, res, pb->var(1), 0.001);
    }
}

const Def* AutoDiffer::zero_pb(const Def* type, const Def* dbg) {
    auto zeropi = createPbType(A,type);
    auto zeropb = world_.nom_filter_lam(zeropi, world_.dbg(dbg));
    auto rmem = zeropb->mem_var();
    auto zero = zero_grad;
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
    dlog(world_,"wrap {} of type {} (node {})",def,def->type(),def->node_name());

    auto dst = j_wrap_convert(def);
    dlog(world_,"{} => {} : {}",def,dst,dst->type());
    src_to_dst_[def]=dst;
    return dst;
}

const Lam* lam_fat_ptr_wrap(World& world, const Lam* lam){
    bool changed = false;
    DefArray doms{lam->num_doms()};
    DefArray src_doms = lam->doms();
    size_t i = 0;
    for(auto dom: src_doms){
        if(auto ptr = isa<Tag::Ptr>(dom)){
            changed = true;
            doms[i] = world.sigma({world.type_int_width(64), ptr});
        }else{
            doms[i] = dom;
        }

        doms[i]->dump();

        i++;
    }

    if(changed){
        auto cn = world.cn(doms);
        Lam* wrapper = world.nom_filter_lam(cn, world.dbg("wrapper"));

        i = 0;
        DefArray arguments{lam->num_doms()};

        for(auto dom: src_doms){
            auto var = wrapper->var(i);
            if(auto ptr = isa<Tag::Ptr>(dom)){
                auto [size, arr] = var->projs<2>();
                arguments[i] = arr;
            }else{
                arguments[i] = var;
            }

            i++;
        }

        wrapper->set_body(world.app(lam, arguments));

        return wrapper;
    }


    return lam;
}


const Def* AutoDiffer::j_wrap_convert(const Def* def) {

    if (auto var = def->isa<Var>()) {
        // variable like whole lambda var should not appear here
        // variables should always be differentiated with their function/lambda context
        THORIN_UNREACHABLE;
    }
    if (auto axiom = def->isa<Axiom>()) {
        // an axiom without application has no meaning as a standalone term
        THORIN_UNREACHABLE;
    }
    if (auto lam = def->isa_nom<Lam>()) {
        // lambda => a function (continuation) (for instance then and else for conditions)
        auto old_pi = lam->type()->as<Pi>();

        auto last_mem=current_mem;

        if( isReturning(lam->type())) {
            auto dst = world_.op_rev_diff(lam);
            // should not be needed => TODO: handle higher order pb correctly in app
            pullbacks_[dst]=zero_pb(lam->type(),world_.dbg("zero_pb_lam"));
            return dst;
        }
        auto args = old_pi->num_doms();

        // take a pullback additionally to the argument
        const Pi* pi;
        if(args==1) {
            pi=old_pi;
        }else{
            pi = world_.cn({world_.type_mem(), old_pi->doms()[1], createPbType(A,old_pi->doms()[1])});
        }
        auto dst = world_.nom_filter_lam(pi, lam->filter(), world_.dbg(lam->name()));
        src_to_dst_[lam->var()] = dst->var();
        if(args>1) {
            pullbacks_[dst->var()] = dst->var(dst->num_vars() - 1); // pullback (for var) is the last argument
        }

        current_mem=dst->mem_var();
        // same as above: jwrap body
        src_to_dst_[lam] = dst; // in case of mutual/indirect recursion
        auto bdy = j_wrap(lam->body());
        dst->set_body(bdy);

        // TODO: need pb?
        // never executed but needed for tuple pb
        pullbacks_[dst] = zero_pb(lam->type(),world_.dbg("zero_pb_lam2"));
        current_mem=last_mem;
        return dst;
    }
    if (auto glob = def->isa<Global>()) {
        // a global is handled like a ptr slot + store with init
        if(auto ptr_ty = isa<Tag::Ptr>(glob->type())) {
            auto dinit = j_wrap(glob->init());
            auto dst=world_.global(dinit,glob->is_mutable(),glob->dbg());

            auto pb = pullbacks_[dinit];
            auto [ty,addr_space] = ptr_ty->arg()->projs<2>();
            auto [pb_mem, pb_ptr] = ptrSlot(ty,current_mem)->projs<2>();
            pointer_map[dst]=pb_ptr;
            auto pb_mem2 = world_.op_store(pb_mem,pb_ptr,pb,world_.dbg("pb_global"));

            auto [pbt_mem,pbt_pb]= reloadPtrPb(pb_mem2,dst,world_.dbg("ptr_slot_pb_loadS"),false);

            current_mem=pbt_mem;
            return dst;
        }
    }

    // handle operations in a hardcoded way
    // we directly implement the pullbacks including the chaining w.r. to the inputs of the function
    if (auto rop = isa<Tag::ROp>(def)) {
        auto ab = j_wrap(rop->arg());
        auto [a, b] = ab->projs<2>();
        if(!pullbacks_.count(a)) {
            pullbacks_[a]= extract_pb(a,ab);
            pullbacks_[b]= extract_pb(b,ab);
        }
        auto dst = j_wrap_rop(ROp(rop.flags()), a, b);
        return dst;
    }
    // conditionals are transformed by the identity (no pullback needed)
    if(auto rcmp = isa<Tag::RCmp>(def)) {
        auto ab = j_wrap(rcmp->arg());
        auto [a, b] = ab->projs<2>();
        auto dst = world_.op(RCmp(rcmp.flags()), nat_t(0), a, b);
        return dst;
    }

    if (auto div = isa<Tag::Div>(def)) {
        // only on integer => no pullback needed
        auto args = j_wrap(div->arg());
        auto dst = world_.app(div->callee(),args);
        pullbacks_[dst]=pullbacks_[args->op(1)]; // the arguments are (mem, int, int)
        return dst;
    }
    if(auto cast = isa<Tag::Bitcast>(def)) {
        // TODO: handle more than identity bitcast
        auto args = j_wrap(cast->arg());
        auto isFatPtr = isFatPtrType(world_,args->type());

        // avoid case distinction
        // copy the bitcast but exchange the arguments with the new ones
        const Def* dst, *dst_pb_org_ty, *arg_pb_ty;
        if(isFatPtr) {
            auto [size,arr] = args->projs<2>();
            auto dst_arr=world_.app(cast->callee(),arr);
            dst_pb_org_ty=dst_arr->type();
            dst = world_.tuple({size,dst_arr});
            arg_pb_ty = arr->type();
        }else {
            dst = world_.app(cast->callee(),args);
            dst_pb_org_ty=dst->type();
            arg_pb_ty = args->type();
        }
        // mostly a zero pb that does not need to be recomputed
        // but for arrays we have to bitcast the argument in opposite direction

        auto arg_pb = pullbacks_[args];
        auto pb_ty = createPbType(A,dst_pb_org_ty);
        auto pb = world_.nom_filter_lam(pb_ty, world_.dbg("pb_bitcast"));
        auto cast_arg = world_.op_bitcast(arg_pb_ty,pb->var(2));
        pb->set_body( world_.app(arg_pb,
         flat_tuple({
            pb->mem_var(),
            world_.tuple({pb->var(1), cast_arg}),
            pb->ret_var()
         }) ));

        pullbacks_[dst]=pb;
//        THORIN_UNREACHABLE;
        return dst;
    }
    if(auto iop = isa<Tag::Conv>(def)) {
        // Unify with wrap
        auto args = j_wrap(iop->arg());
        // avoid case distinction
        auto dst = world_.app(iop->callee(),args);
        // a zero pb but do not recompute
        pullbacks_[dst]=pullbacks_[args];
        return dst;
    }
    if(auto iop = isa<Tag::Wrap>(def)) {
        auto args = j_wrap(iop->arg());
        // avoid case distinction
        auto dst = world_.app(iop->callee(),args);
        // a zero pb but do not recompute
        pullbacks_[dst]=pullbacks_[args->op(0)];
        return dst;
    }
    // TODO: more general integer operations
    if(auto icmp = isa<Tag::ICmp>(def)) {
        auto ab = j_wrap(icmp->arg());
        auto [a, b] = ab->projs<2>();
        auto dst = world_.op(ICmp(icmp.flags()), a, b);
        return dst;
    }
    if (auto alloc = isa<Tag::Alloc>(def)) {
        // inner callee type:  array: size; type
        auto alloc_arg = alloc->callee()->as<App>()->arg();
        auto [base_type,gid] = alloc_arg->projs<2>();
        auto [_,ptr_type]=alloc->type()->projs<2>();
        auto type=base_type;
        auto mem_arg = j_wrap(alloc->arg());

        auto dst_alloc = world_.op_alloc(type,mem_arg,alloc->dbg());
        auto [r_mem,arr] = dst_alloc->projs<2>();
        auto size=type->as<Arr>()->shape();
        auto int_size=world_.op_bitcast(world_.type_int_width(64),size);
        auto dst_fat_ptr=world_.tuple({int_size,arr});
        auto dst=world_.tuple({r_mem,dst_fat_ptr});
        current_mem = r_mem;

        // no shadow needed
        // TODO: shadow if one handles alloc like a ptr (for definite)
        auto pb = zero_pb(ptr_type,world_.dbg("pb_alloc"));
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
        auto ptr_ty = as<Tag::Ptr>(lea->type());
        auto [ty,addr_space] = ptr_ty->arg()->projs<2>();
        auto fat_ptr=j_wrap(lea->arg(0));
        auto [arr_size,arr] = fat_ptr->projs<2>();
        auto idx = j_wrap(lea->arg(1)); // not necessary
        auto dst = world_.op_lea(arr,idx);
        auto [arr_ty, arr_addr_space] = as<Tag::Ptr>(arr->type())->arg()->projs<2>();
        auto pi = createPbType(A,ty);
        auto pb = world_.nom_filter_lam(pi, world_.dbg("pb_lea"));
        auto arr_size_nat = world_.op_bitcast(world_.type_nat(),arr_size);
        auto arr_sized_ty=world_.arr(arr_size_nat,arr_ty->as<Arr>()->body())->as<Arr>();
        auto ptr_arr_sized_ty = world_.type_ptr(arr_sized_ty);
        // TODO: merge with ZERO?

        auto [mem2,ptr_arr]=world_.op_alloc(arr_sized_ty,pb->mem_var())->projs<2>();
        auto shape=arr_sized_ty->shape();
        auto body = arr_sized_ty->body();
        auto [mem3, body_lit] = ZERO(world_,mem2,body);
        auto init=world_.pack(shape,body_lit);
        auto mem4=world_.op_store(mem3,ptr_arr,init);
        assert(pullbacks_.count(fat_ptr) && "arr from lea should already have an pullback");
//        type_dump(world_,"fat_ptr",fat_ptr);
//        type_dump(world_,"pb of fat_ptr",pullbacks_[fat_ptr]);
        auto ptr_arr_idef = pullbacks_[fat_ptr]->type()->as<Pi>()->dom(2);
//        auto ptr_arr_idef = pullbacks_[fat_ptr]->type()->as<Pi>()->dom(1)->op(1); // if single fat ptr pb is non_flat
//        type_dump(world_,"ptr_arr_idef",ptr_arr_idef);
        auto ptr_arr_arg = world_.op_bitcast(ptr_arr_idef,ptr_arr);
        auto fat_ptr_arr_arg = world_.tuple({arr_size,ptr_arr_arg});
//        dlog(world_,"lea on ptr_arr_arg {} of type {} with idx {} : {}",ptr_arr_arg,ptr_arr_arg->type(),idx,idx->type());
        auto scal_ptr = world_.op_lea(ptr_arr_arg,idx);
        auto v = pb->var(1);
        auto mem5 = world_.op_store(mem4,scal_ptr,v);
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
        auto callee = app->callee();
        auto arg = app->arg();
        // Handle binary operations
        if (auto inner = callee->isa<App>()) {
            // Take care of binary operations
            if (auto inner2_app = inner->callee()->isa<App>()) {
                if(auto axiom = inner2_app->callee()->isa<Axiom>(); axiom && axiom->tag()==Tag::RevDiff) {
                    auto d_arg = j_wrap(arg); // args to call diffed function
                    auto fn = inner->arg(); // function to diff
                    // inner2_app = rev_diff <...>
                    // callee = rev_diff ... fun
                    return world_.app(callee,d_arg);
                }
            }

            if (auto axiom = inner->callee()->isa<Axiom>()) {
                if (axiom->tag() == Tag::Slot) {
                    auto [ty, addr_space] = inner->arg()->projs<2>();
                    auto j_args = j_wrap(arg);
                    auto [mem, num] = j_args->projs<2>();

                    auto [pb_mem, pb_ptr] = ptrSlot(ty,mem)->projs<2>();

                    auto dst = world_.op_slot(ty,pb_mem);
                    auto [dst_mem, dst_ptr] = dst->projs<2>();
                    pointer_map[dst]=pb_ptr; // for mem tuple extract
                    pointer_map[dst_ptr]=pb_ptr;
                    // to prevent error in load for tuple pb
                    auto [nmem,pb_loaded]=reloadPtrPb(dst_mem,dst_ptr,world_.dbg("ptr_slot_pb_loadL"),true);
                    dst_mem=nmem;
                    pullbacks_[dst]=pb_loaded;
                    current_mem=dst_mem;
                    return dst;
                }
                if (axiom->tag() == Tag::Store) {
                    auto j_args = j_wrap(arg);
                    auto [mem, ptr, val] = j_args->projs<3>();
                    assert(pointer_map.count(ptr) && "ptr should have a shadow slot at a store location");
                    auto pb=pullbacks_[val];

                    auto pb_mem = world_.op_store(mem,pointer_map[ptr],pb,world_.dbg("pb_store"));

                    // necessary to access ptr pb when calling
                    // all other accesses are handled by load of the ptr with corresponding pb slot load
                    auto [pbt_mem,pbt_pb]= reloadPtrPb(pb_mem,ptr,world_.dbg("ptr_slot_pb_loadS"),false);
                    auto dst = world_.op_store(pbt_mem,ptr,val);
                    pullbacks_[dst]=pb; // should be unused
                    current_mem=dst;
                    return dst;
                }
                if (axiom->tag() == Tag::Load) {
                    auto j_args = j_wrap(arg);
                    auto [mem, ptr] = j_args->projs<2>();
                    // TODO: where is pullbacks_[ptr] set to a nullptr? (happens in conditional stores to slot)
                    // TODO: why do we need or not need this load
                        auto [nmem,pb_loaded]=reloadPtrPb(mem,ptr,world_.dbg("ptr_slot_pb_loadL"),true);
                        mem=nmem;
                    auto dst = world_.op_load(mem,ptr);
                    auto [dst_mem,dst_val] = dst->projs<2>();
                    pullbacks_[dst]=pb_loaded; // tuple extract [mem,...]
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
            const Def* dst_callee;

            auto d_arg = j_wrap(arg);
            if(auto cal_lam=callee->isa<Lam>(); cal_lam && !cal_lam->is_set()) {
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
                // f*
                auto gradlam=world_.nom_filter_lam(pbTy, world_.dbg("dummy"));

                // new augmented lam f' to replace old one
                auto lam=world_.nom_filter_lam(augTy,world_.dbg("dummy"));
                auto lam2 = world_.nom_filter_lam(cal_lam->doms().back()->as<Pi>(),world_.dbg("dummy"));

                auto wrapped_cal_lam = lam_fat_ptr_wrap(world_, cal_lam);
                derive_external(wrapped_cal_lam, gradlam, lam, lam2);

                lam->set_debug_name(cal_lam->name() + "_diff_impl");
                lam2->set_debug_name(lam->name() + "_cont");
                gradlam->set_debug_name(cal_lam->name() + "_pb");
                auto callee_arguments = world_.tuple( flat_tuple({
                     lam->mem_var(),
                     world_.tuple(vars_without_mem_cont(lam)),
                     lam2
                }));

                lam->set_body( world_.app(
                    wrapped_cal_lam,
                    callee_arguments
                ));

                lam2->set_body( world_.app(
                    lam->ret_var(),
                    {
                        lam2->mem_var(),
                        world_.tuple(vars_without_mem_cont(lam2)),
                        gradlam
                    }
                ));
                dst_callee = lam;
            }else {
                if(callee->isa<Lam>()) {
                    auto ret_ty = callee->type()->as<Pi>()->doms().back()->as<Pi>();
                    if(ret_ty->num_doms()==1) {
                        // function is cn[mem] => only side effects
                        // and it is a called function
                        // => do nothing
                        auto dst = world_.app(
                           callee,
                            d_arg
                        );
                        pullbacks_[dst] = pullbacks_[d_arg];
                        return dst;
                    }else {
                        dst_callee = world_.op_rev_diff(callee);
                    }
                }else{
                    dst_callee= j_wrap(callee);
                }
            }
            auto m = d_arg->proj(0);
            auto num_projs = d_arg->num_projs();
            auto ret_arg = d_arg->proj(num_projs-1);
            auto arg= world_.tuple(
                d_arg->projs().skip(1,1)
            );
            auto pbT = dst_callee->type()->as<Pi>()->doms().back()->as<Pi>();
            auto chained = world_.nom_filter_lam(pbT, world_.dbg("phi_chain"));
            auto arg_pb = pullbacks_[d_arg]; // Lam
            auto ret_pb = chained->var(chained->num_vars() - 1);
            auto chain_pb = chain(ret_pb,arg_pb);
            // TODO
            chained->set_body( world_.app(
                ret_arg,
                flat_tuple({
                    chained->mem_var(),
                    world_.tuple(vars_without_mem_cont(chained)),
                    chain_pb
                })
                ));
            // TODO ?
            auto dst = world_.app(dst_callee, flat_tuple({m,arg,chained}));
            pullbacks_[dst] = pullbacks_[d_arg];
            return dst;
        }else {
            auto d_arg = j_wrap(arg);
            auto d_callee= j_wrap(callee); // invokes lambda
            if(pullbacks_.count(d_arg)) {
            }
            const Def* ad_args;
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
                auto count=getDim(d_arg);
                ad_args = world_.tuple(
                    DefArray(
                    count+1,
                    [&](auto i) {
                        if (i<count) {
                            return world_.extract(d_arg, (u64)i, world_.dbg("ad_arg"));
                        } else {
                            return pullbacks_[d_arg];
                        }
                    }
                ));
            }else {
                // var (lambda completely with all arguments) and other (non tuple)
                ad_args = d_arg;
            }
            return world_.app(d_callee, ad_args);
        }
    }

    if (auto tuple = def->isa<Tuple>()) {
        auto tuple_dim=getDim(tuple->type());
        DefArray ops{tuple_dim, [&](auto i) { return tuple->proj(i); }};
        auto dst = j_wrap_tuple(ops);
        return dst;
    }

    if (auto pack = def->isa<Pack>()) {
        // no pullback for pack needed
        auto dim = as_lit(pack->type()->arity());
        auto tup=DefArray(
            dim,
            [&](auto) {
              return pack->body();
            });
        return j_wrap_tuple(tup);
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
        auto jeidx= j_wrap(extract->index());
        auto jtup = j_wrap(extract->tuple());
        auto dst = world_.extract_unsafe(jtup, jeidx,extract->dbg());
        if(!isa<Tag::Mem>(dst->type())) {
            pullbacks_[dst] = extract_pb(dst,jtup);
        }
        return dst;
    }

    if (auto insert = def->isa<Insert>()) {
        // TODO: currently not handled but not difficult
        // important note: we need the pullback w.r. to the tuple and element
        // construction needs careful consideration of modular basic pullbacks
        // see notes on paper for correct code
        return world_.insert(j_wrap(insert->tuple()), insert->index(), j_wrap(insert->value()));
    }

    if (auto lit = def->isa<Lit>()) {
        // a literal (number) has a zero pullback
        pullbacks_[lit] = zero_pb(lit->type(), world_.dbg("zero_pb_lit"));
        return lit;
    }
    THORIN_UNREACHABLE;
}
// translates operation calls and creates the pullbacks
const Def* AutoDiffer::j_wrap_rop(ROp op, const Def* a, const Def* b) {
    // build up pullback type for this expression
    auto o_type = a->type(); // type of the operation
    auto pbpi = createPbType(A,o_type);
    auto pbT = pullbacks_[a]->type()->as<Pi>()->doms().back()->as<Pi>(); // TODO: create using A
    auto pb = world_.nom_filter_lam(pbpi, world_.dbg("phi_"));

    // shortened pullback type => takes pullback result (A) And continues
    // always expand operation pullbacks
    auto middle = world_.nom_filter_lam(pbT, world_.dbg("phi_middle"));
    auto end = world_.nom_filter_lam(pbT, world_.dbg("phi_end"));

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

    auto adiff = world_.tuple(vars_without_mem_cont(middle));
    auto bdiff = world_.tuple(vars_without_mem_cont(end));
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
        auto isClosure = app->num_args()>1;

        auto fun_arg = isClosure ? app->arg(1) : app->arg(0);
        auto src_lam = fun_arg->as_nom<Lam>();
        auto src_pi = src_lam->type();
        // function to differentiate
        // this should be something like `cn[:mem, r32, cn[:mem, r32]]`
        auto& world = src_lam->world();

        // We get for `A -> B` the type `A -> (B * (B -> A))`.
        //  i.e. cn[:mem, A, [:mem, B]] ---> cn[:mem, A, cn[:mem, B, cn[:mem, B, cn[:mem, A]]]]
        //  take input, return result and return a function (pullback) taking z and returning the derivative
        const Pi* dst_pi;
        if(isClosure)
            dst_pi = app->type()->op(1)->as<Pi>();
        else
            dst_pi = app->type()->as<Pi>(); // multi dim as array
        auto dst_lam = world.nom_filter_lam(dst_pi, src_lam->filter(), world.dbg("top_level_rev_diff_" + src_lam->name())); // copy the unfold filter
        // use src to not dilute tangent transformation with left type transformation (only matters for arrays)
        auto A = world.params_without_return_continuation(src_pi); // input variable(s) => possible a pi type (array)

        // is cn[mem, B0, ..., Bm, pb] => skip mem and pb
        auto B = world.params_without_return_continuation(dst_pi->dom()->ops().back()->as<Pi>());
        // The actual AD, i.e. construct "sq_cpy"
        Def2Def src_to_dst;
        // src_to_dst maps old definitions to new ones
        // here we map the arguments of the lambda

        src_to_dst[src_lam] = dst_lam;
        src_to_dst[src_lam->var()] = dst_lam->var();
        auto differ = AutoDiffer{world, src_to_dst, A};
        dst_lam->set_body(differ.reverse_diff(src_lam));

        auto dst=isClosure ? world.insert(app->arg(),1,dst_lam) : dst_lam;
        return dst;
    }}}
    return def;
}

}