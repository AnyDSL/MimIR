#ifndef THORIN_BE_LL_LL_H
#define THORIN_BE_LL_LL_H

#include <ostream>
#include <string>

namespace thorin {

class World;

namespace ll {

void emit(World&, std::ostream&);

int compile(World&, std::string stem);
int compile(World&, std::string ll, std::string out);
int compile_and_run(World&, std::string stem, std::string args = {});

} // namespace ll

} // namespace thorin

#endif
