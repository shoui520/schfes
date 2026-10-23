#pragma once
// npps4/system/effort.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <tuple>
#include <vector>

namespace npps::system::effort {

// Never null; throws std::invalid_argument.
const db::effort::LiveEffortPointBoxSpec *get_effort_spec(Context &context, int64_t live_effort_point_box_spec_id);
// add_effort with the reward items kept as objects. EffortPointInfo.rewards holds a JSON snapshot, but callers
// (lbonus, live) mutate the items afterwards (reward_box_flag, UnitItem.unit_owning_user_id) and Python's
// EffortPointInfo shares those objects: after mutating, call refresh_rewards().
struct EffortResult {
    std::vector<EffortPointInfo> infos;
    // rewards[i] are the items of infos[i]
    std::vector<std::vector<model::Ptr<item_model::Item>>> rewards;
    int64_t offer_limited_box_id = 0;

    // infos[i].rewards = dump(rewards[i])
    void refresh_rewards();
};
EffortResult add_effort_items(Context &context, db::main::User &user, int64_t amount);

// Returns (effort point infos, offer_limited_box_id).
std::tuple<std::vector<EffortPointInfo>, int64_t> add_effort(Context &context, db::main::User &user, int64_t amount);

}  // namespace npps::system::effort
