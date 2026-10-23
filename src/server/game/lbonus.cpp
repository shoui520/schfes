// npps4/game/lbonus.py
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/effort.hpp"
#include "system/item.hpp"
#include "system/lbonus.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/strings.hpp"
#include "system/user.hpp"

namespace npps::game::lbonus {

namespace sach = system::achievement;
using ItemList = std::vector<model::Ptr<system::item_model::Item>>;

static LoginBonusResponse lbonus_execute(Context &context) {
    int64_t server_timestamp = util::time();
    auto current_datetime = util::datetime(server_timestamp);

    int64_t next_year = current_datetime.year, next_month_num = current_datetime.month + 1;
    if (next_month_num > 12) {
        next_year = next_year + 1;
        next_month_num = 1;
    }

    auto *current_user = system::user::get_current(context);
    auto current_month = system::lbonus::get_calendar(context, current_datetime.year, current_datetime.month);
    auto next_month = system::lbonus::get_calendar(context, next_year, next_month_num);
    int64_t login_count = system::lbonus::get_login_count(context, *current_user);
    auto lbonuses_day =
        system::lbonus::days_login_bonus(context, *current_user, current_datetime.year, current_datetime.month);
    int64_t present_count = system::reward::count_presentbox(context, *current_user);

    bool has_lbonus = lbonuses_day.count(current_datetime.day) != 0;
    Json get_item;
    int64_t add_effort_amount = 0;
    sach::AchievementContext achievement_list;
    std::vector<ItemList> accomplished_rewards;
    std::vector<ItemList> unaccomplished_rewards;
    if (!has_lbonus) {
        auto reward_item = current_month[current_datetime.day - 1].item;
        add_effort_amount = 100000;
        login_count = login_count + 1;
        present_count = present_count + 1;
        auto reward_message = strings::format_simple(strings::get("lbonus", 12), (int64_t)current_datetime.month,
                                                     (int64_t)current_datetime.day);
        // TODO: Add unit support
        system::reward::add_item(context, *current_user, reward_item, reward_message.first, reward_message.second);
        system::lbonus::mark_login_bonus(context, *current_user, current_datetime.year, current_datetime.month,
                                         current_datetime.day);
        auto get_item_obj = model::make<system::item_model::Item>();
        get_item_obj->add_type = reward_item->add_type;
        get_item_obj->item_id = reward_item->item_id;
        get_item_obj->amount = reward_item->amount;
        system::item::update_item_category_id(context, get_item_obj);
        get_item = get_item_obj->to_json();
        lbonuses_day.insert(current_datetime.day);
        // Do achievement check
        sach::AchievementUpdateLoginBonus update;
        update.login_days = login_count;
        achievement_list.extend(sach::check(context, *current_user, {update}));
        for (auto *ach : achievement_list.accomplished)
            accomplished_rewards.push_back(sach::get_achievement_rewards(context, *ach));
        for (auto *ach : achievement_list.new_)
            unaccomplished_rewards.push_back(sach::get_achievement_rewards(context, *ach));
        accomplished_rewards = system::advanced::fixup_achievement_reward(context, *current_user, accomplished_rewards);
        unaccomplished_rewards =
            system::advanced::fixup_achievement_reward(context, *current_user, unaccomplished_rewards);
        sach::process_achievement_reward(context, *current_user, achievement_list.accomplished, accomplished_rewards);
    }

    // Modify current_month
    for (int64_t day : lbonuses_day) current_month[day - 1].received = true;

    auto effort_result = system::effort::add_effort_items(context, *current_user, add_effort_amount);
    for (const auto &rewards : effort_result.rewards) {
        for (const auto &r : rewards) {
            auto add_result = system::advanced::add_item(context, *current_user, r);
            if (!add_result.success) {
                auto msg = strings::format_simple(strings::get("lbonus", 12), (int64_t)current_datetime.month,
                                                  (int64_t)current_datetime.day);
                system::reward::add_item(context, *current_user, r, msg.first, msg.second);
                r->reward_box_flag = true;
            }
        }
    }
    effort_result.refresh_rewards();

    std::string current_date = std::to_string(current_datetime.year) + "-" + std::to_string(current_datetime.month) +
                               "-" + std::to_string(current_datetime.day);

    LoginBonusResponse response;
    response.calendar_info.current_date = current_date;
    response.calendar_info.current_month.year = current_datetime.year;
    response.calendar_info.current_month.month = current_datetime.month;
    response.calendar_info.current_month.days = std::move(current_month);
    response.calendar_info.next_month.year = next_year;
    response.calendar_info.next_month.month = next_month_num;
    response.calendar_info.next_month.days = std::move(next_month);
    response.calendar_info.get_item = std::move(get_item);
    response.ad_info = system::ad_model::AdInfo();
    response.total_login_info.login_count = login_count;
    response.license_lbonus_list = Json::array();  // TODO
    response.start_dash_sheets = Json::array();    // TODO
    response.effort_point = std::move(effort_result.infos);
    response.limited_effort_box = Json::array();  // TODO
    response.accomplished_achievement_list =
        sach::to_game_representation(context, achievement_list.accomplished, accomplished_rewards);
    response.unaccomplished_achievement_cnt = sach::get_achievement_count(context, *current_user, false);
    response.after_user_info = system::user::get_user_info(context, *current_user);
    response.added_achievement_list = sach::to_game_representation(context, achievement_list.new_, unaccomplished_rewards);
    response.new_achievement_cnt = (int64_t)achievement_list.new_.size();
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = present_count;
    return response;
}
NPPS_ENDPOINT("lbonus", "execute", User, lbonus_execute, .batchable = false, .xmc_verify = XMCVerifyMode::CROSS,
              .exclude_none = true);

}  // namespace npps::game::lbonus
