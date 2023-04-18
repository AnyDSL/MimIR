#pragma once

#include <cassert>
#include <cstring>

namespace thorin {

namespace detail {
template<class T>
concept Nodeable = requires(T n) {
                       T::Node;
                       n.node();
                   };
} // namespace detail

/// Inherit from this class using [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern),
/// for some nice `dynamic_cast`-style wrappers.
template<class B>
class RuntimeCast {
public:
    // clang-format off
    /// `static_cast` with debug check.
    template<class T> T* as() { assert(isa<T>()); return  static_cast<T*>(this); }

    /// `dynamic_cast`.
    /// If @p T isa thorin::detail::Nodeable, it will use `node()`, otherwise a `dynamic_cast`.
    template<class T>
    T* isa() {
        if constexpr (detail::Nodeable<T>) {
            return static_cast<B*>(this)->node() == T::Node ? static_cast<T*>(this) : nullptr;
        } else {
            return dynamic_cast<T*>(static_cast<B*>(this));
        }
    }

    /// Yields `B*` if it is *either* @p T or @p U and `nullptr* otherwise.
    template<class T, class U> B* isa() { return (isa<T>() || isa<U>()) ? static_cast<B*>(this) : nullptr; }

    template<class T         > const T*  as() const { return const_cast<RuntimeCast*>(this)->template  as<T   >(); } ///< `const` version.
    template<class T         > const T* isa() const { return const_cast<RuntimeCast*>(this)->template isa<T   >(); } ///< `const` version.
    template<class T, class U> const B* isa() const { return const_cast<RuntimeCast*>(this)->template isa<T, U>(); } ///< `const` version.
    // clang-format on
};

} // namespace thorin
