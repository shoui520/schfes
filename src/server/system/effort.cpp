// npps4/system/effort.py
#include "system/effort.hpp"

#include "external/live_box_drop.hpp"
#include "system/advanced.hpp"
#include "system/const.hpp"
#include "system/item.hpp"

#include <algorithm>
#include <stdexcept>

namespace npps::system::effort {

const db::effort::LiveEffortPointBoxSpec *get_effort_spec(Context &context, int64_t live_effort_point_box_spec_id) {
    const auto *effort_spec = context.db.effort.get<db::effort::LiveEffortPointBoxSpec>(live_effort_point_box_spec_id);
    if (effort_spec == nullptr)
        throw std::invalid_argument("invalid live_effort_point_box_spec_id " +
                                    std::to_string(live_effort_point_box_spec_id));
    return effort_spec;
}

void EffortResult::refresh_rewards() {
    for (size_t i = 0; i < infos.size() && i < rewards.size(); i++) infos[i].rewards = model::dump(rewards[i]).elements();
}

EffortResult add_effort_items(Context &context, db::main::User &user, int64_t amount) {
    EffortResult out;
    int64_t current_amount = std::min(amount, const_::MAX_EFFORT_SCORE);
    int64_t oldvalue = 0;
    const db::effort::LiveEffortPointBoxSpec *effort_spec = nullptr;

    // TODO: Properly handle limited effort box
    while (true) {
        bool is_limited = user.limited_effort_event_id > 0;
        effort_spec = get_effort_spec(context, user.live_effort_point_box_spec_id);
        int64_t capacity = is_limited ? effort_spec->limited_capacity : effort_spec->capacity;
        oldvalue = is_limited ? user.current_limited_effort_point : user.current_live_effort_point;
        int64_t newvalue = std::min(oldvalue + current_amount, capacity);
        int64_t addedvalue = newvalue - oldvalue;
        current_amount = current_amount - addedvalue;

        if (is_limited) user.current_limited_effort_point = newvalue;
        else user.current_live_effort_point = newvalue;

        if (newvalue >= capacity) {
            // Give reward
            auto drop_box_result = external::live_box_drop::process_effort_box(
                context, user.live_effort_point_box_spec_id, user.limited_effort_event_id, amount);

            std::vector<model::Ptr<item_model::Item>> reward_list;
            for (const auto &[add_type, item_id, item_count, additional_data] : drop_box_result.rewards) {
                item_model::BaseItem base;
                base.add_type = (const_::ADD_TYPE)add_type;
                base.item_id = item_id;
                base.amount = item_count;
                base.extra_data = additional_data;
                auto reward_data = advanced::deserialize_item_data(context, base);
                item::update_item_category_id(context, reward_data);
                reward_list.push_back(reward_data);
            }

            // FIXME: This does NOT handle limited effort box properly!
            EffortPointInfo info;
            info.live_effort_point_box_spec_id = user.live_effort_point_box_spec_id;
            info.capacity = effort_spec->capacity;
            info.before = oldvalue;
            info.after = newvalue;
            out.infos.push_back(std::move(info));
            out.rewards.push_back(std::move(reward_list));

            if (is_limited) {
                user.limited_effort_event_id = 0;
                user.current_limited_effort_point = 0;
            } else {
                user.live_effort_point_box_spec_id = drop_box_result.new_live_effort_point_box_spec_id;
                user.current_live_effort_point = 0;
            }

            if (drop_box_result.offer_limited_effort_event_id > 0)
                out.offer_limited_box_id = drop_box_result.offer_limited_effort_event_id;
        }

        if (current_amount <= 0) break;
    }

    EffortPointInfo last;
    last.live_effort_point_box_spec_id = user.live_effort_point_box_spec_id;
    last.capacity = effort_spec->capacity;
    last.before = oldvalue;
    last.after = user.current_live_effort_point;
    out.infos.push_back(std::move(last));
    out.rewards.emplace_back();
    out.refresh_rewards();
    context.db.main.flush();
    return out;
}

std::tuple<std::vector<EffortPointInfo>, int64_t> add_effort(Context &context, db::main::User &user, int64_t amount) {
    EffortResult r = add_effort_items(context, user, amount);
    return {std::move(r.infos), r.offer_limited_box_id};
}

}  // namespace npps::system::effort
