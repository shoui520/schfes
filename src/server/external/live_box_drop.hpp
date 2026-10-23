#pragma once
// external/live_box_drop.py: Live Show! Reward Box drops (config.get_live_box_drop_protocol()).
#include "app/context.hpp"
#include "core/json.hpp"

#include <tuple>
#include <vector>

namespace npps::external::live_box_drop {

struct ProcessResult {
    int64_t new_live_effort_point_box_spec_id = 0;
    int64_t offer_limited_effort_event_id = 0;
    // (add_type, item_id, amount, extra data or null)
    std::vector<std::tuple<int64_t, int64_t, int64_t, Json>> rewards;
};

ProcessResult process_effort_box(Context &context, int64_t current_live_effort_point_box_spec_id,
                                 int64_t current_limited_effort_event_id, int64_t score);

}  // namespace npps::external::live_box_drop
