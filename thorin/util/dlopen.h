#ifndef THORIN_UTIL_DLOPEN_H
#define THORIN_UTIL_DLOPEN_H

#include <string>
#include <filesystem>
#include <optional>

namespace thorin {
void* load_library(const std::string& filename);
void* get_symbol_from_library(void* handle, const std::string& symbol_name);
std::optional<std::filesystem::path> get_path_to_current_executable();
void close_library(void* handle);
}

#endif // THORIN_UTIL_DLOPEN_H