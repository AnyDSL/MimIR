#pragma once
#include <optional>
#include <vector>

#include "thorin/world.h"

namespace thorin::regex {
std::vector<std::pair<nat_t, nat_t>> merge_ranges(World& w, const std::vector<std::pair<nat_t, nat_t>>& old_ranges);

} // namespace thorin::regex
