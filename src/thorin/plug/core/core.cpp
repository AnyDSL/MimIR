#include "thorin/plug/core/core.h"

#include <thorin/config.h>

#include <thorin/pass/pass.h>

#include "thorin/plug/core/be/ll.h"

using namespace thorin;
using namespace thorin::plug;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"core", [](Normalizers& normalizers) { core::register_normalizers(normalizers); }, nullptr,
            [](Backends& backends) { backends["ll"] = &ll::emit; }};
}

namespace thorin::plug::core {

template<bool up> const Sigma* convert(const TBound<up>* b) {
    auto& w = b->world();

    if constexpr (up) {
        nat_t align = 0;
        nat_t size  = 0;

        for (auto op : b->ops()) {
            auto a = Lit::isa(w.call(trait::align, op));
            auto s = Lit::isa(w.call(trait::size, op));
            if (!a || !s) return nullptr;

            align = std::max(align, *a);
            size  = std::max(size, *s);
        }

        assert(size % align == 0);
        auto arr = w.arr(size / align, w.Int(align * 8_u64));

        return w.sigma({w.Idx(b->num_ops()), arr})->template as<Sigma>();
    } else {
        return w.sigma(b->ops())->template as<Sigma>();
    }
}

#ifndef DOXYGEN
template const Sigma* convert(const TBound<false>*);
template const Sigma* convert(const TBound<true>*);
#endif

} // namespace thorin::plug::core
