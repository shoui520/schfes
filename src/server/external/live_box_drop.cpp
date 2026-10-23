// external/live_box_drop.py
#include "external/live_box_drop.hpp"

#include "data/data.hpp"

#include <map>

namespace npps::external::live_box_drop {

using Reward = std::tuple<int64_t, int64_t, int64_t, Json>;

// Rebuilt whenever server_data changes (data::get() keeps old versions alive, so the address identifies a version).
static const data::ServerData *server_data = nullptr;
static std::map<int64_t, std::vector<Reward>> cached_live_drops;

static void refresh_drop_cache() {
    const data::ServerData *current_server_data = &data::get();

    if (current_server_data != server_data) {
        for (const auto &[live_effort_point_box_spec_id, drops] : current_server_data->live_effort_drops) {
            std::vector<Reward> result;

            for (const auto &drop : drops) {
                // BaseItemWithWeight has no "extra" attribute, so this is always None.
                for (int64_t i = 0; i < drop.weight; i++)
                    result.emplace_back((int64_t)drop.add_type, drop.item_id, drop.amount, Json());
            }

            cached_live_drops[live_effort_point_box_spec_id] = std::move(result);
        }

        server_data = current_server_data;
    }
}

static std::vector<Reward> get_reward_from_box(int64_t live_effort_point_box_spec_id, int64_t amount) {
    refresh_drop_cache();

    std::vector<Reward> result;
    auto it = cached_live_drops.find(live_effort_point_box_spec_id);
    if (it != cached_live_drops.end()) {
        for (int64_t i = 0; i < amount; i++) result.push_back(util::rng().choice(it->second));
    }

    return result;
}

ProcessResult process_effort_box(Context &context, int64_t current_live_effort_point_box_spec_id,
                                 int64_t current_limited_effort_event_id, int64_t score) {
    (void)current_limited_effort_event_id;
    auto effort_spec_list = context.db.effort.select<db::effort::LiveEffortPointBoxSpec>("ORDER BY capacity DESC");

    // Determine the current effort box
    const db::effort::LiveEffortPointBoxSpec *current_box = nullptr;
    for (const auto *effort_spec : effort_spec_list) {
        if (effort_spec->live_effort_point_box_spec_id == current_live_effort_point_box_spec_id) {
            current_box = effort_spec;
            break;
        }
    }

    int64_t next_box_id = 1;
    // Determine the next effort box
    for (const auto *effort_spec : effort_spec_list) {
        if ((score * 4) >= effort_spec->capacity) {
            next_box_id = effort_spec->live_effort_point_box_spec_id;
            break;
        }
    }

    ProcessResult result;
    result.new_live_effort_point_box_spec_id = next_box_id;
    result.offer_limited_effort_event_id = 0;
    if (current_box != nullptr)
        result.rewards = get_reward_from_box(current_box->live_effort_point_box_spec_id, current_box->num_rewards);
    return result;
}

}  // namespace npps::external::live_box_drop
