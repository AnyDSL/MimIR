#include "dialects/core/core.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/core/be/ll/ll.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"core", nullptr,
            [](Passes& passes) {
                register_pass<core::partial_eval_pass, PartialEval>(passes);
                register_pass<core::beta_red_pass, BetaRed>(passes);
                register_pass<core::eta_red_pass, EtaRed>(passes);

                register_pass<core::scalerize_no_arg_pass, Scalerize>(passes);
                register_pass<core::tail_rec_elim_no_arg_pass, TailRecElim>(passes);
                register_pass<core::lam_spec_pass, LamSpec>(passes);
                register_pass<core::ret_wrap_pass, RetWrap>(passes);

                register_pass_with_arg<core::eta_exp_pass, EtaExp, EtaRed>(passes);
                register_pass_with_arg<core::scalerize_pass, Scalerize, EtaExp>(passes);
                register_pass_with_arg<core::tail_rec_elim_pass, TailRecElim, EtaRed>(passes);
            },
            [](Backends& backends) { backends["ll"] = &ll::emit; },
            [](Normalizers& normalizers) { core::register_normalizers(normalizers); }};
}

namespace thorin::core {

template<bool up>
const Sigma* convert(const TBound<up>* b) {
    auto& w = b->world();

    if constexpr (up) {
        nat_t align = 0;
        nat_t size  = 0;

        for (auto op : b->ops()) {
            auto a = isa_lit(core::op(trait::align, op));
            auto s = isa_lit(core::op(trait::size, op));
            if (!a || !s) return nullptr;

            align = std::max(align, *a);
            size  = std::max(size, *s);
        }

        assert(size % align == 0);
        auto arr = w.arr(size / align, w.type_int(align * 8_u64));

        return w.sigma({w.type_idx(b->num_ops()), arr})->template as<Sigma>();
    } else {
        return w.sigma(b->ops())->template as<Sigma>();
    }
}

template const Sigma* convert(const TBound<false>*);
template const Sigma* convert(const TBound<true>*);

} // namespace thorin::core
