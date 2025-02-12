#pragma once

#include <cstddef>
#include <cstdint>

#include <type_traits>

namespace mim {

/// @name Simple Hash
/// See [Wikipedia](https://en.wikipedia.org/wiki/MurmurHash).
///@{
/// Use for a single value to hash.
inline uint32_t murmur3(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline uint64_t splitmix64(uint64_t h) {
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9;
    h ^= h >> 27;
    h *= 0x94d049bb133111eb;
    h ^= h >> 31;
    return h;
}

inline size_t hash(size_t h) {
    if constexpr (sizeof(size_t) == 4)
        return murmur3(h);
    else if constexpr (sizeof(size_t) == 8)
        return splitmix64(h);
    else
        static_assert("unsupported size of size_t");
}
///@}

/// @name FNV-1 Hash
/// See [Wikipedia](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1_hash).
///@{
/// [Magic numbers](http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-var) for FNV-1 hash.
template<class T> struct FNV1 {};

template<> struct FNV1<uint32_t> {
    static const uint32_t offset = 2166136261;
    static const uint32_t prime  = 16777619;
};

template<> struct FNV1<uint64_t> {
    static const uint64_t offset = 14695981039346656037ULL;
    static const uint64_t prime  = 1099511628211ULL;
};

template<class T> size_t hash_combine(size_t seed, T v) {
    static_assert(std::is_signed<T>::value || std::is_unsigned<T>::value, "please provide your own hash function");

    size_t val = v;
    for (size_t i = 0; i < sizeof(T); ++i) {
        size_t octet = val & size_t(0xff); // extract lower 8 bits
        seed ^= octet;
        seed *= FNV1<size_t>::prime;
        val >>= size_t(8);
    }
    return seed;
}

template<class T> size_t hash_begin(T val) { return hash_combine(FNV1<size_t>::offset, val); }
inline size_t hash_begin() { return FNV1<size_t>::offset; }
///@}

} // namespace mim
