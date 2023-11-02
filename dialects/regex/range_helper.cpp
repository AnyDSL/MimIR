#include "range_helper.h"

namespace thorin::regex {
std::optional<std::pair<nat_t, nat_t>> merge_ranges(std::pair<nat_t, nat_t> a, std::pair<nat_t, nat_t> b) {
    if (!(a.second + 1 < b.first || b.second + 1 < a.first)) {
        return {
            {std::min(a.first, b.first), std::max(a.second, b.second)}
        };
    }
    return {};
}

std::vector<std::pair<nat_t, nat_t>> merge_ranges(World& w, const std::vector<std::pair<nat_t, nat_t>>& old_ranges) {
    std::vector<std::pair<nat_t, nat_t>> new_ranges;
    for (auto it = old_ranges.begin(); it != old_ranges.end(); ++it) {
        auto current_range = *it;
        w.DLOG("old range: {}-{}", current_range.first, current_range.second);
        for (auto inner = it + 1; inner != old_ranges.end(); ++inner)
            if (auto merged = merge_ranges(current_range, *inner)) current_range = *merged;

        std::vector<std::vector<std::pair<nat_t, nat_t>>::iterator> de_duplicate;
        for (auto inner = new_ranges.begin(); inner != new_ranges.end(); ++inner) {
            if (auto merged = merge_ranges(current_range, *inner)) {
                current_range = *merged;
                de_duplicate.push_back(inner);
            }
        }
        for (auto dedup : de_duplicate) {
            w.DLOG("dedup {}-{}", current_range.first, current_range.second);
            new_ranges.erase(dedup);
        }
        w.DLOG("new range: {}-{}", current_range.first, current_range.second);
        new_ranges.push_back(std::move(current_range));
    }
    return new_ranges;
}
} // namespace thorin::regex
