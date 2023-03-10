#pragma once

#ifdef _WIN32
#    define THORIN_WHICH "where"
#else
#    define THORIN_WHICH "which"
#endif

#include <filesystem>
#include <optional>
#include <string>

namespace thorin {

namespace fs = std::filesystem;

namespace sys {

/// Yields `std::nullopt` if an error occurred.
std::optional<fs::path> path_to_curr_exe();

/// Executes command @p cmd.
/// @returns the output as string.
std::string exec(std::string cmd);

std::string find_cmd(std::string);

/// Wraps `std::system` and makes the return value usable.
int system(std::string);

/// Wraps sys::system and puts `.exe` at the back (Windows) and `./` at the front (otherwise) of @p cmd.
int run(std::string cmd, std::string args = {});

} // namespace sys
} // namespace thorin
