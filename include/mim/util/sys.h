#pragma once

#include <filesystem>
#include <optional>
#include <string>

#ifdef _WIN32
#    define MIM_WHICH "where"
#else
#    define MIM_WHICH "which"
#endif

namespace mim {

namespace fs = std::filesystem;

namespace sys {

std::optional<fs::path> path_to_curr_exe(); ///< Yields `std::nullopt` if an error occurred.

/// Executes command @p cmd.
/// @returns the output as string.
std::string exec(std::string cmd);

std::string find_cmd(std::string);

/// Wraps `std::system` and makes the return value usable.
int system(std::string);

/// Wraps sys::system and puts `.exe` at the back (Windows) and `./` at the front (otherwise) of @p cmd.
int run(std::string cmd, std::string args = {});

/// Returns the @p path as `std::string` and escapes all whitespaces with backslash.
std::string escape(const std::filesystem::path& path);

} // namespace sys
} // namespace mim
