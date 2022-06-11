#include "thorin/util/hash.h"

namespace thorin {

hash_t hash(const char* s) {
    hash_t seed = thorin::hash_begin();
    for (const char* p = s; *p != '\0'; ++p) seed = thorin::hash_combine(seed, *p);
    return seed;
}

hash_t hash(std::string_view s) {
    hash_t seed = thorin::hash_begin(s.size());
    for (auto c : s) seed = thorin::hash_combine(seed, c);
    return seed;
}

} // namespace thorin
