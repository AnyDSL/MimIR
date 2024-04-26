#include "thorin/util/dbg.h"

#include <fe/loc.cpp.h>

namespace thorin {

void Error::clear() {
    num_errors_   = 0;
    num_warnings_ = 0;
    num_notes_    = 0;
    msgs_.clear();
}

/// If errors occured, claim them and throw; if warnings occured, claim them and report to @p os.
void Error::ack(std::ostream& os) {
    auto e = std::move(*this);
    if (e.num_errors() != 0) throw e;
    if (e.num_warnings() != 0) print(os, "{} warning(s) encountered\n{}", e.num_warnings(), e);
}

} // namespace thorin
