#pragma once

#include <memory>
#include <string>
#include <vector>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include "thorin/config.h"
#include "thorin/def.h"

namespace thorin {

class PipelineBuilder;

/// @name Plugin Interface
///@{
using Normalizers = absl::flat_hash_map<flags_t, NormalizeFn>;
/// `axiom ↦ (pipeline part) × (axiom application) → ()` <br/>
/// The function should inspect App%lication to construct the Pass/Phase and add it to the pipeline.
using Passes   = absl::flat_hash_map<flags_t, std::function<void(World&, PipelineBuilder&, const Def*)>>;
using Backends = absl::btree_map<std::string, void (*)(World&, std::ostream&)>;
///@}

extern "C" {
/// Basic info and registration function pointer to be returned from a specific plugin.
/// Use Driver to load such a plugin.
struct Plugin {
    using Handle = std::unique_ptr<void, void (*)(void*)>;

    const char* plugin_name; ///< Name of the Plugin.

    /// Callback for registering the mapping from axiom ids to normalizer functions in the given @p normalizers map.
    void (*register_normalizers)(Normalizers& normalizers);
    /// Callback for registering the Plugin's callbacks for the pipeline extension points.
    void (*register_passes)(Passes& passes);
    /// Callback for registering the mapping from backend names to emission functions in the given @p backends map.
    void (*register_backends)(Backends& backends);
};

/// @name Plugin Interface
///@{
/// @see Plugin

/// To be implemented and exported by a plugin.
/// @returns a filled Plugin.
THORIN_EXPORT thorin::Plugin thorin_get_plugin();
///@}
}

struct Annex {
    Annex(Sym plugin, Sym tag, flags_t tag_id)
        : plugin(plugin)
        , tag(tag)
        , tag_id(tag_id) {}

    Sym plugin;
    Sym tag;
    flags_t tag_id;
    std::deque<std::deque<Sym>> subs; ///< List of subs which is a list of aliases.
    Sym normalizer;
    bool pi = false;

    /// @name Mangling Plugin Name
    ///@{
    static constexpr size_t Max_Plugin_Size = 8;
    static constexpr plugin_t Global_Plugin = 0xffff'ffff'ffff'0000_u64;

    /// Mangles @p s into a dense 48-bit representation.
    /// The layout is as follows:
    /// ```
    /// |---7--||---6--||---5--||---4--||---3--||---2--||---1--||---0--|
    /// 7654321076543210765432107654321076543210765432107654321076543210
    /// Char67Char66Char65Char64Char63Char62Char61Char60|---reserved---|
    /// ```
    /// The `reserved` part is used for the Axiom::tag and the Axiom::sub.
    /// Each `Char6x` is 6-bit wide and hence a plugin name has at most Axiom::Max_Plugin_Size = 8 chars.
    /// It uses this encoding:
    /// | `Char6` | ASCII   |
    /// |---------|---------|
    /// | 1:      | `_`     |
    /// | 2-27:   | `a`-`z` |
    /// | 28-53:  | `A`-`Z` |
    /// | 54-63:  | `0`-`9` |
    /// The 0 is special and marks the end of the name if the name has less than 8 chars.
    /// @returns `std::nullopt` if encoding is not possible.
    static std::optional<plugin_t> mangle(Sym s);

    /// Reverts an Axiom::mangle%d string to a Sym.
    /// Ignores lower 16-bit of @p u.
    static Sym demangle(World&, plugin_t u);

    static std::array<Sym, 3> split(World&, Sym);
    ///@}

    /// @name Helpers for Matching
    ///@{
    /// These are set via template specialization.

    /// Number of Axiom::sub%tags.
    template<class Id>
    static constexpr size_t Num = size_t(-1);

    /// @see Axiom::base.
    template<class Id>
    static constexpr flags_t Base = flags_t(-1);
    ///@}
};

} // namespace thorin
