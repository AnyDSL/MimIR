#ifndef THORIN_UTIL_SYS_H
#define THORIN_UTIL_SYS_H

#ifdef _WIN32
#    define THORIN_WHICH "where"
#else
#    define THORIN_WHICH "which"
#endif

#include <filesystem>
#include <optional>
#include <string>

namespace thorin::sys {

/// @returns `std::nullopt` if an error occurred.
std::optional<std::filesystem::path> path_to_curr_exe();

/// Executes command @p cmd. @returns the output as string.
std::string exec(const char* cmd);

} // namespace thorin::sys

#endif
