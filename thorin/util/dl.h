#ifndef THORIN_UTIL_DL_H
#define THORIN_UTIL_DL_H

#include <string>
#include <filesystem>
#include <optional>

#include "thorin/error.h"

namespace thorin::dl {

class Error : public std::runtime_error {
public:
    Error(const std::string& what_arg)
        : std::runtime_error(what_arg) {}
};

std::string_view prefix();    ///< `"lib"` or `""`
std::string_view extension(); ///< `".dll"`, `".dylib"`, or `".so"`

void* open(const std::string& filename);
void* get(void* handle, const std::string& symbol_name);
void close(void* handle);

/// @returns `std::nullopt` if an error occurred.
std::optional<std::filesystem::path> get_path_to_current_executable();

} // namespace thorin::dl

#endif // THORIN_UTIL_DL_H
