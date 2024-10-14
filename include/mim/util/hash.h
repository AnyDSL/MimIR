#pragma once

#include "mim/util/types.h"

namespace mim {

/// @name Aliases for some Base Types
///@{
using hash_t = uint32_t;
///@}

/// @name Murmur3 Hash
/// See [Wikipedia](https://en.wikipedia.org/wiki/MurmurHash).
///@{
inline hash_t murmur_32_scramble(hash_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

inline hash_t murmur3(hash_t h, uint32_t key) {
    h ^= murmur_32_scramble(key);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
    return h;
}

inline hash_t murmur3(hash_t h, uint64_t key) {
    hash_t k = hash_t(key);
    h ^= murmur_32_scramble(k);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
    k = hash_t(key >> 32);
    h ^= murmur_32_scramble(k);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
    return h;
}

inline hash_t murmur3_rest(hash_t h, uint8_t key) {
    h ^= murmur_32_scramble(key);
    return h;
}

inline hash_t murmur3_rest(hash_t h, uint16_t key) {
    h ^= murmur_32_scramble(key);
    return h;
}

inline hash_t murmur3_finalize(hash_t h, hash_t len) {
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

/// Use for a single value to hash.
inline hash_t murmur3(hash_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
///@}

/// @name FNV-1 Hash
/// See [Wikipedia](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1_hash).
///@{
/// [Magic numbers](http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-var) for FNV-1 hash.
struct FNV1 {
    static const hash_t offset = 2166136261_u32;
    static const hash_t prime  = 16777619_u32;
};

/// Returns a new hash by combining the hash @p seed with @p val.
template<class T> hash_t hash_combine(hash_t seed, T v) {
    static_assert(std::is_signed<T>::value || std::is_unsigned<T>::value, "please provide your own hash function");

    hash_t val = v;
    for (hash_t i = 0; i < sizeof(T); ++i) {
        hash_t octet = val & 0xff_u32; // extract lower 8 bits
        seed ^= octet;
        seed *= FNV1::prime;
        val >>= 8_u32;
    }
    return seed;
}

template<class T> hash_t hash_combine(hash_t seed, T* val) { return hash_combine(seed, uintptr_t(val)); }

template<class T, class... Args> hash_t hash_combine(hash_t seed, T val, Args&&... args) {
    return hash_combine(hash_combine(seed, val), std::forward<Args>(args)...);
}

template<class T> hash_t hash_begin(T val) { return hash_combine(FNV1::offset, val); }
inline hash_t hash_begin() { return FNV1::offset; }
///@}

/// @name String Hashing
///@{
hash_t hash(const char*);
hash_t hash(std::string_view);
///@}

} // namespace mim
