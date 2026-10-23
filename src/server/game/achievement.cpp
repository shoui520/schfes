// npps4/game/achievement.py
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::game::achievement {

namespace sach = system::achievement;
using ItemList = std::vector<model::Ptr<system::item_model::Item>>;

static AchievementUnaccomplishedResponse achievement_unaccomplishlist(Context &context) {
    auto *current_user = system::user::get_current(context);
    auto filter_ids = sach::get_achievement_filter_ids(context);

    AchievementUnaccomplishedResponse result;
    for (int64_t fcat : filter_ids) {
        auto unaccomplished = sach::get_unclaimed_achievements_by_filter_id(context, *current_user, fcat);
        std::vector<ItemList> unaccomplished_rewards;
        for (auto *ach : unaccomplished) unaccomplished_rewards.push_back(sach::get_achievement_rewards(context, *ach));
        auto ach_list = sach::to_game_representation(context, unaccomplished, unaccomplished_rewards);
        AchievementUnaccomplishedFilter f;
        f.filter_category_id = fcat;
        f.count = (int64_t)ach_list.size();
        f.achievement_list = std::move(ach_list);
        result.push_back(std::move(f));
    }

    return result;
}
NPPS_ENDPOINT("achievement", "unaccomplishList", User, achievement_unaccomplishlist);

static AchievementUnaccomplishedResponse achievement_initialaccomplishlist(Context &context) {
    auto *current_user = system::user::get_current(context);
    auto filter_ids = sach::get_achievement_filter_ids(context);

    AchievementUnaccomplishedResponse result;
    for (int64_t fcat : filter_ids) {
        auto accomplished = sach::get_accomplished_achievements_by_filter_id(context, *current_user, fcat);
        std::vector<ItemList> accomplished_rewards;
        for (auto *ach : accomplished) accomplished_rewards.push_back(sach::get_achievement_rewards(context, *ach));
        accomplished_rewards = system::advanced::fixup_achievement_reward(context, *current_user, accomplished_rewards);
        auto ach_list = sach::to_game_representation(context, accomplished, accomplished_rewards);
        AchievementUnaccomplishedFilter f;
        f.filter_category_id = fcat;
        f.count = (int64_t)ach_list.size();
        f.achievement_list = std::move(ach_list);
        result.push_back(std::move(f));
    }

    return result;
}
NPPS_ENDPOINT("achievement", "initialAccomplishedList", User, achievement_initialaccomplishlist);

static AchievementRewardOpenResponse achievement_rewardopen(Context &context,
                                                            const AchievementRewardOpenRequest &request) {
    auto *current_user = system::user::get_current(context);
    int64_t achievement_id = request.accomplish_id;
    auto *ach = sach::get_achievement(context, *current_user, achievement_id);
    const auto *ach_info = sach::get_achievement_info(context, ach->achievement_id);
    auto rewards = sach::get_achievement_rewards(context, *ach);
    auto new_rewards = system::advanced::fixup_achievement_reward(context, *current_user, {rewards});
    rewards = new_rewards[0];

    auto before_user = system::user::get_user_info(context, *current_user);

    for (const auto &reward_item : rewards) {
        auto success = system::advanced::add_item(context, *current_user, reward_item);
        if (!success) {
            reward_item->reward_box_flag = true;
            std::string fallback_name = "Achievement #" + std::to_string(ach->achievement_id);
            std::string title = ach_info->title && !ach_info->title->empty() ? *ach_info->title : fallback_name;
            std::string title_en = ach_info->title_en && !ach_info->title_en->empty() ? *ach_info->title_en : title;
            system::reward::add_item(context, *current_user, reward_item, title, title_en);
        }
    }

    // Mark claimed
    sach::mark_achievement_reward_claimed(context, *ach);
    context.db.main.flush();
    int64_t achievement_count = sach::get_achievement_count(context, *current_user, false);

    AchievementRewardOpenResponse response;
    response.accomplished_achievement_list = {};
    response.unaccomplished_achievement_cnt = achievement_count;
    response.added_achievement_list = {};
    response.new_achievement_cnt = achievement_count;
    response.before_user_info = before_user;
    response.after_user_info = system::user::get_user_info(context, *current_user);
    for (const auto &r : rewards) response.reward_item_list.push_back(model::dump(r));
    response.unit_support_list = system::unit::get_unit_support_list_response(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("achievement", "rewardOpen", User, achievement_rewardopen);

static AchievementRewardOpenAllResponse achievement_rewardopenall(Context &context) {
    auto *current_user = system::user::get_current(context);
    auto achievements = sach::get_unclaimed_achievements(context, *current_user, true);
    int64_t opened_count = 0;
    ItemList reward_item_list;
    auto before_user = system::user::get_user_info(context, *current_user);

    for (auto *ach : achievements) {
        const auto *ach_info = sach::get_achievement_info(context, ach->achievement_id);
        auto rewards = sach::get_achievement_rewards(context, *ach);
        auto new_rewards = system::advanced::fixup_achievement_reward(context, *current_user, {rewards});

        for (const auto &reward_item : new_rewards[0]) {
            auto success = system::advanced::add_item(context, *current_user, reward_item);
            if (!success) {
                reward_item->reward_box_flag = true;
                std::string fallback_name = "Achievement #" + std::to_string(ach->achievement_id);
                std::string title = ach_info->title && !ach_info->title->empty() ? *ach_info->title : fallback_name;
                std::string title_en = ach_info->title_en && !ach_info->title_en->empty() ? *ach_info->title_en : title;
                system::reward::add_item(context, *current_user, reward_item, title, title_en);
            }
            reward_item_list.push_back(reward_item);
        }

        sach::mark_achievement_reward_claimed(context, *ach);
        opened_count = opened_count + 1;
    }

    int64_t achievement_count = sach::get_achievement_count(context, *current_user, false);
    AchievementRewardOpenAllResponse response;
    response.accomplished_achievement_list = {};
    response.unaccomplished_achievement_cnt = achievement_count;
    response.added_achievement_list = {};
    response.new_achievement_cnt = achievement_count;
    response.before_user_info = before_user;
    response.after_user_info = system::user::get_user_info(context, *current_user);
    for (const auto &r : reward_item_list) response.reward_item_list.push_back(model::dump(r));
    response.unit_support_list = system::unit::get_unit_support_list_response(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    response.is_last = true;
    response.opened_num = opened_count;
    return response;
}
NPPS_ENDPOINT("achievement", "rewardOpenAll", User, achievement_rewardopenall);

}  // namespace npps::game::achievement
