#include "mim/util/hash.h"

namespace mim {

hash_t hash(const char* s) {
    hash_t seed = mim::hash_begin();
    for (const char* p = s; *p != '\0'; ++p) seed = mim::hash_combine(seed, *p);
    return seed;
}

hash_t hash(std::string_view s) {
    hash_t seed = mim::hash_begin(s.size());
    for (auto c : s) seed = mim::hash_combine(seed, c);
    return seed;
}

} // namespace mim
