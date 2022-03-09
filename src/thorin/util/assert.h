#ifndef THORIN_UTIL_ASSERT_H
#define THORIN_UTIL_ASSERT_H

#include <cassert>

namespace thorin {

// see https://stackoverflow.com/a/65258501
#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)
[[noreturn]] inline __attribute__((always_inline)) void unreachable() { __builtin_unreachable(); }
#elif defined(_MSC_VER) // MSVC
[[noreturn]] __forceinline void unreachable() { __assume(false); }
#else                   // ???
inline void unreachable() {}
#endif

#if (defined(__clang__) || defined(__GNUC__)) && (defined(__x86_64__) || defined(__i386__))
inline void breakpoint() { asm("int3"); }
#else
inline void breakpoint() {
    volatile int* p = nullptr;
    *p              = 42;
}
#endif

} // namespace thorin

#ifndef NDEBUG
#    define assert_unused(x) assert(x)
#else
#    define assert_unused(x) ((void)(0 && (x)))
#endif

#endif
