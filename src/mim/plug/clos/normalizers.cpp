#include "mim/plug/clos/clos.h"

namespace mim::plug::clos {

template<attr o> Ref normalize_clos(Ref, Ref, Ref arg) { return o == attr::bottom ? arg : Ref{}; }

MIM_clos_NORMALIZER_IMPL

} // namespace mim::plug::clos
