#ifndef THORIN_BE_LL_LL_H
#define THORIN_BE_LL_LL_H

#include <ostream>
#include <string>

namespace thorin {

class World;

namespace ll {

void emit(World&, std::ostream&);

int compile(World&, const std::string& stem);
int compile(World&, const std::string& ll, const std::string& out);

} // namespace ll

} // namespace thorin

#endif
