#include "thorin/pass/rw/peephole.h"

#define dlog(world,...) world.DLOG(__VA_ARGS__)
#define type_dump(world,name,d) world.DLOG("{} {} : {}",name,d,d->type())

namespace thorin {

const Def* Peephole::rewrite(const Def* def) {
    World& world_=RWPass<Lam>::curr_nom()->world();
    if (auto rop = isa<Tag::ROp>(def)) {
        type_dump(world_,"ROp",rop);
        auto [a, b] = rop->arg()->projs<2>();
        type_dump(world_,"  a",a);
        type_dump(world_,"  b",b);

        switch (ROp(rop.flags())) {
            case ROp::add: {
                dlog(world_,"  add");
                if(auto lit = a->isa<Lit>()){
                    dlog(world_,"  add left lit");
                    if(lit->get<double_t>()==0) {
                        dlog(world_,"  add left 0");
                        return b;
                    }
                }
                // right
                // both lit
            }
                // mult 0/1
            default: {}
        }
        return def;
    }
    return def;
}

}
