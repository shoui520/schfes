// npps4/game/reward.py
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::game::reward {

namespace achievement = system::achievement;
namespace advanced = system::advanced;
namespace reward_system = system::reward;
namespace user = system::user;

// IncentiveItem.from_incentive
static model::Ptr<IncentiveItem> incentive_item_from_incentive(Context &context, const db::main::Incentive &i) {
    auto obj = model::make<IncentiveItem>(IncentiveItem::make(
        i.id, i.item_id, (const_::ADD_TYPE)i.add_type, i.amount, 0,
        context.is_lang_jp() || i.message_en.value_or("").empty() ? i.message_jp : *i.message_en,
        util::timestamp_to_datetime(i.insert_date),
        i.expire_date == 0 ? std::string("Forever") : util::timestamp_to_datetime(i.expire_date)));
    // Add extra fields
    if (obj->add_type == const_::ADD_TYPE::UNIT) {
        // NPPS4 validates json.dumps(extra_data) (a str) as a model, which always fails, so it is always EMPTY.
        system::unit_model::UnitExtraData extra_data = system::unit_model::UnitExtraData_EMPTY;
        auto unit_item = system::unit::create_unit_item(context, i.item_id, i.amount, extra_data);
        system::unit::populate_unit_item_to_other(unit_item, *obj);
    }
    return obj;
}

static bool order_has(RewardOrder order, RewardOrder flag) { return any(order & flag); }

static RewardListResponse reward_rewardlist(Context &context, const RewardListRequest &request) {
    db::main::User *current_user = user::get_current(context);
    auto incentive = reward_system::get_presentbox(context, *current_user, request, request.offset, 20,
                                                   order_has(request.order, RewardOrder::ORDER_ASCENDING),
                                                   order_has(request.order, RewardOrder::BY_EXPIRE_DATE));
    int64_t incentive_total_count = reward_system::count_presentbox(context, *current_user, &request);

    RewardListResponse response;
    response.item_count = incentive_total_count;
    response.order = request.order;
    for (db::main::Incentive *i : incentive) response.items.push_back(incentive_item_from_incentive(context, *i));
    return response;
}
NPPS_ENDPOINT("reward", "rewardList", User, reward_rewardlist);

static model::Ptr<RewardIncentiveItem> make_incentive_item(const model::Ptr<system::item_model::Item> &item_data,
                                                           int64_t incentive_id, bool id_first) {
    Json merged = Json::object();
    if (id_first) merged["incentive_id"] = incentive_id;
    Json item_json = item_data->to_json();
    for (const auto &m : item_json.members()) merged[m.first] = m.second;
    if (!id_first) merged["incentive_id"] = incentive_id;
    return model::make<RewardIncentiveItem>(model::parse<RewardIncentiveItem>(merged));
}

static void process_achievement_rewards(Context &context, db::main::User &current_user,
                                        const achievement::AchievementContext &achievement_list,
                                        std::vector<std::vector<model::Ptr<system::item_model::Item>>> &accomplished,
                                        std::vector<std::vector<model::Ptr<system::item_model::Item>>> &unaccomplished) {
    // Give achievement rewards
    for (db::main::Achievement *ach : achievement_list.accomplished)
        accomplished.push_back(achievement::get_achievement_rewards(context, *ach));
    for (db::main::Achievement *ach : achievement_list.new_)
        unaccomplished.push_back(achievement::get_achievement_rewards(context, *ach));
    accomplished = advanced::fixup_achievement_reward(context, current_user, accomplished);
    unaccomplished = advanced::fixup_achievement_reward(context, current_user, unaccomplished);
    achievement::process_achievement_reward(context, current_user, achievement_list.accomplished, accomplished);
}

static RewardOpenResponse reward_open(Context &context, const RewardOpenRequest &request) {
    // https://github.com/Salaron/alay/blob/master/src/modules/api/reward/open.ts
    db::main::User *current_user = user::get_current(context);
    db::main::Incentive *incentive = reward_system::get_incentive(context, *current_user, request.incentive_id);
    if (incentive == nullptr) throw error::by_code(error::ERROR_CODE_INCENTIVE_NONE);

    auto before_user = user::get_user_info(context, *current_user);
    auto item_data = reward_system::resolve_incentive(context, *current_user, *incentive);
    auto add_result = advanced::add_item(context, *current_user, item_data);
    auto supp_units = system::unit::get_all_supporter_unit(context, *current_user);
    bool success = add_result.success;

    std::vector<achievement::AchievementUpdate> album_trigger, achievement_update;
    if (success) {
        reward_system::remove_incentive(context, *incentive);
        if (item_data->add_type == const_::ADD_TYPE::UNIT && album_trigger.empty()) {
            album_trigger.push_back(achievement::AchievementUpdateNewUnit{});
            album_trigger.push_back(achievement::AchievementUpdateUnitRankUp{{}});
        }

        achievement_update.push_back(
            achievement::AchievementUpdateItemCollect{item_data->add_type, item_data->item_id, item_data->amount});
    } else {
        throw error::by_code(error::ERROR_CODE_LIMIT_OVER);
    }

    std::vector<achievement::AchievementUpdate> updates = album_trigger;
    updates.insert(updates.end(), achievement_update.begin(), achievement_update.end());
    auto achievement_list = achievement::check(context, *current_user, updates);

    std::vector<std::vector<model::Ptr<system::item_model::Item>>> accomplished_rewards, unaccomplished_rewards;
    process_achievement_rewards(context, *current_user, achievement_list, accomplished_rewards, unaccomplished_rewards);

    RewardOpenResponse response;
    for (const auto &supp : supp_units) {
        system::unit_model::SupporterInfoResponse s;
        s.unit_id = std::get<0>(supp);
        s.amount = std::get<1>(supp);
        response.unit_support_list.push_back(s);
    }
    response.before_user_info = before_user;
    response.after_user_info = user::get_user_info(context, *current_user);
    response.accomplished_achievement_list =
        achievement::to_game_representation(context, achievement_list.accomplished, accomplished_rewards);
    response.unaccomplished_achievement_cnt = achievement::get_achievement_count(context, *current_user, false);
    response.added_achievement_list =
        achievement::to_game_representation(context, achievement_list.new_, unaccomplished_rewards);
    response.new_achievement_cnt = (int64_t)achievement_list.new_.size();
    response.opened_num = success;
    if (success) response.success.push_back(make_incentive_item(item_data, request.incentive_id, true));
    response.present_cnt = reward_system::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("reward", "open", User, reward_open);

static RewardOpenAllResponse reward_openall(Context &context, const RewardListRequest &request) {
    db::main::User *current_user = user::get_current(context);
    auto before_user = user::get_user_info(context, *current_user);
    int64_t total_presentbox = reward_system::count_presentbox(context, *current_user, &request);
    auto incentives = reward_system::get_presentbox(context, *current_user, request, 0, 1000,
                                                    order_has(request.order, RewardOrder::ORDER_ASCENDING),
                                                    order_has(request.order, RewardOrder::BY_EXPIRE_DATE));
    int64_t reward_count = (int64_t)incentives.size();
    std::vector<model::Ptr<RewardIncentiveItem>> reward_item_list;
    bool need_check_unit_ach = false;
    std::vector<achievement::AchievementUpdate> achievement_update;

    for (db::main::Incentive *incentive : incentives) {
        auto item_data = reward_system::resolve_incentive(context, *current_user, *incentive);
        auto add_result = advanced::add_item(context, *current_user, item_data);
        bool success = add_result.success;

        if (success) {
            int64_t incentive_id = incentive->id;
            reward_item_list.push_back(make_incentive_item(item_data, incentive_id, false));
            reward_system::remove_incentive(context, *incentive);
            if (item_data->add_type == const_::ADD_TYPE::UNIT) need_check_unit_ach = true;

            achievement_update.push_back(
                achievement::AchievementUpdateItemCollect{item_data->add_type, item_data->item_id, item_data->amount});
        }
    }

    if (need_check_unit_ach) {
        achievement_update.push_back(achievement::AchievementUpdateNewUnit{});
        achievement_update.push_back(achievement::AchievementUpdateUnitRankUp{{}});
    }

    auto achievement_list = achievement::check(context, *current_user, achievement_update);

    std::vector<std::vector<model::Ptr<system::item_model::Item>>> accomplished_rewards, unaccomplished_rewards;
    process_achievement_rewards(context, *current_user, achievement_list, accomplished_rewards, unaccomplished_rewards);

    int64_t opened = (int64_t)reward_item_list.size();

    RewardOpenAllResponse response;
    response.accomplished_achievement_list =
        achievement::to_game_representation(context, achievement_list.accomplished, accomplished_rewards);
    response.unaccomplished_achievement_cnt = achievement::get_achievement_count(context, *current_user, false);
    response.added_achievement_list =
        achievement::to_game_representation(context, achievement_list.new_, unaccomplished_rewards);
    response.new_achievement_cnt = (int64_t)achievement_list.new_.size();
    response.reward_num = total_presentbox;
    response.opened_num = opened;
    response.total_num = reward_count;
    response.order = (int64_t)request.order;
    response.upper_limit = false;
    response.reward_item_list = std::move(reward_item_list);
    response.before_user_info = before_user;
    response.after_user_info = user::get_user_info(context, *current_user);
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = reward_system::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("reward", "openAll", User, reward_openall);

static RewardHistoryResponse reward_rewardhistory(Context &, const RewardHistoryRequest &request) {
    // TODO
    util::log("reward.rewardHistory stub: %s", request.to_json().dump().c_str());
    RewardHistoryResponse response;
    response.item_count = 0;
    return response;
}
NPPS_ENDPOINT("reward", "rewardHistory", User, reward_rewardhistory);

}  // namespace npps::game::reward
