#include "mim/tuple.h"
#include "mim/world.h"

#include "mim/util/types.h"

#include "mim/plug/mem/autogen.h"
#include "mim/plug/spirv/autogen.h"

namespace mim::plug::spirv {

// MIM_spirv_NORMALIZER_IMPL

// const Def* normalize_wrap(const Def* type, const Def*, const Def* arg) {
//     auto& world      = type->world();
//     auto [Gi, Go, f] = arg->projs<3>();

//     // length has to be known at compile time
//     if (!Gi->isa<Tuple>()) return nullptr;

//     // auto name_ = std::format("{}_", f->node_name()
//     auto mem_t = world.call<mem::M>(0);
//     Lam* f_    = world.mut_fun({mem_t, Gi, Go}, mem_t)->set("main");

//     auto [mem, gis, gos, ret] = f_->vars<4>();

//     std::vector<const Def*> is{};
//     for (auto gi : gis->ops()) {
//         auto [mem_, i] = world.call<spirv::load>(world.tuple({mem, gi}))->projs<2>();
//         mem            = mem_;
//         is.push_back(i);
//     }

//     auto res = world.app(f, is)->projs(gos->num_projs());
//     for (nat_t index = 0; index < gos->num_projs(); index++)
//         mem = world.call<spirv::store>(world.tuple({mem, gos->proj(index)}), res[index]);
//     f_->app(false, ret, mem);
//     f_->externalize();

//     return f_;
// }

} // namespace mim::plug::spirv
