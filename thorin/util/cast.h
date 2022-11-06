#pragma once

#include <cassert>
#include <cstring>

namespace thorin {

/// A bitcast.
template<class D, class S>
inline D bitcast(const S& src) {
    D dst;
    auto s = reinterpret_cast<const void*>(&src);
    auto d = reinterpret_cast<void*>(&dst);

    if constexpr (sizeof(D) == sizeof(S)) memcpy(d, s, sizeof(D));
    if constexpr (sizeof(D) < sizeof(S)) memcpy(d, s, sizeof(D));
    if constexpr (sizeof(D) > sizeof(S)) {
        memset(d, 0, sizeof(D));
        memcpy(d, s, sizeof(S));
    }
    return dst;
}

/// Handy wrapper for `dynamic_cast`.
template<class Base>
class RTTICast {
public:
    // clang-format off
    template<class T>       T* isa()       {                   return dynamic_cast<      T*>(static_cast<      Base*>(this)); } ///< `dynamic_cast`.
    template<class T> const T* isa() const {                   return dynamic_cast<const T*>(static_cast<const Base*>(this)); } ///< As above - `const` version.
    template<class T>       T*  as()       { assert(isa<T>()); return  static_cast<      T*>(this); }                           ///< `static_cast` with debug check.
    template<class T> const T*  as() const { assert(isa<T>()); return  static_cast<const T*>(this); }                           ///< As above - `const` version.
    // clang-format on
};

} // namespace thorin
