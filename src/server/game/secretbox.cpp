// npps4/game/secretbox.py
#include "app/config.hpp"
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/const.hpp"
#include "system/item.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/secretbox.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace npps::game::secretbox {

namespace s_user = ::npps::system::user;
namespace s_item = ::npps::system::item;
namespace s_unit = ::npps::system::unit;
namespace s_secretbox = ::npps::system::secretbox;
namespace s_achievement = ::npps::system::achievement;
namespace s_reward = ::npps::system::reward;
namespace s_museum = ::npps::system::museum;
namespace s_advanced = ::npps::system::advanced;
namespace unit_model = ::npps::system::unit_model;
namespace item_model = ::npps::system::item_model;

static SecretboxAllResponse secretbox_all(Context &context) {
    db::main::User *current_user = s_user::get_current(context);
    auto item_list = s_item::get_item_list(context, *current_user);
    SecretboxAllResponse r;
    r.use_cache = 0;
    r.is_unit_max = s_unit::is_unit_max(context, *current_user);
    r.item_list = std::get<0>(item_list);
    r.member_category_list = s_secretbox::get_all_secretbox_data_response(context, *current_user);
    return r;
}
NPPS_ENDPOINT("secretbox", "all", User, secretbox_all);

static constexpr bool HIDDEN_UR_UMI_RARE = false;
static constexpr int64_t LOWEST_RARITY_SORT_ORDER[] = {1, 2, 3, 5, 4};

static SecretboxPonResponse secretbox_gachapon(Context &context, const SecretboxPonRequest &request) {
    db::main::User *current_user = s_user::get_current(context);
    auto [secretbox_id, button_index, cost_index] = s_secretbox::decode_cost_id(request.id);

    const data::schema::SecretboxData *secretbox_data_ptr;
    try {
        secretbox_data_ptr = &s_secretbox::get_secretbox_data(secretbox_id);
    } catch (const std::out_of_range &e) {
        util::log("uh oh secretbox_data %lld %s", (long long)secretbox_id, e.what());
        throw error::by_code(error::ERROR_CODE_SECRET_BOX_NOT_EXIST);
    }
    const data::schema::SecretboxData &secretbox_data = *secretbox_data_ptr;

    const data::schema::SecretboxButton *secretbox_button_ptr;
    const data::schema::SecretboxCost *secretbox_cost_ptr;
    try {
        secretbox_button_ptr = &s_secretbox::get_secretbox_button(secretbox_data, button_index);
        int64_t ci = cost_index - 1;  // Python list indexing
        int64_t n = (int64_t)secretbox_button_ptr->costs.size();
        if (ci < 0) ci += n;
        if (ci < 0 || ci >= n) throw std::out_of_range("list index out of range");
        secretbox_cost_ptr = &secretbox_button_ptr->costs[(size_t)ci];
    } catch (const std::out_of_range &e) {
        util::log("uh oh secretbox_button %lld %lld %s", (long long)button_index, (long long)cost_index, e.what());
        throw error::by_code(error::ERROR_CODE_SECRET_BOX_COST_TYPE_IS_NOT_SPECIFIED);
    }
    const data::schema::SecretboxButton &secretbox_button = *secretbox_button_ptr;
    const data::schema::SecretboxCost &secretbox_cost = *secretbox_cost_ptr;

    // Check currency
    int64_t cost_amount = util::ceil_mul(secretbox_cost.cost_amount, config::get().secretbox_cost_multiplier);
    if (s_secretbox::get_user_currency(context, *current_user, secretbox_cost.cost_type, secretbox_cost.cost_item_id) <
        cost_amount)
        throw error::by_code(error::ERROR_CODE_SECRET_BOX_REMAINING_COST_IS_NOT_ENOUGH);

    // Capture current user info
    ::npps::system::user::UserInfoData before_user_info = s_user::get_user_info(context, *current_user);

    // Roll units
    std::vector<int64_t> unit_roll = s_secretbox::roll_units(
        secretbox_id, secretbox_button.unit_count, secretbox_button.guaranteed_rarity,
        secretbox_button.guarantee_specific_rarity_amount, secretbox_button.rate_modifier);
    std::vector<model::Ptr<unit_model::UnitSupportItem>> unit_data_list;
    int64_t current_unit_count = s_unit::count_units(context, *current_user, true);

    bool umi_rare_mode = false;
    int64_t lowest_rarity = 5;  // sort order
    if (HIDDEN_UR_UMI_RARE && unit_roll.size() == 1) {
        const db::unit::Unit *unit_info = s_unit::get_unit_info(context, unit_roll[0]);
        if (unit_info && unit_info->rarity == 2 && (unit_info->unit_type_id == 4 || unit_info->unit_type_id == 94))
            umi_rare_mode = true;
    }

    int64_t unit_expiry = util::time() + const_::COMMON_UNIT_EXPIRY;
    for (int64_t unit_id : unit_roll) {
        s_unit::QuickAddResult reward_data = s_unit::quick_create_by_unit_add(context, *current_user, unit_id);
        if (!reward_data.as_item_reward->unit_rarity_id) throw std::logic_error("unit_rarity_id is None");

        auto unit_item = std::dynamic_pointer_cast<unit_model::UnitItem>(reward_data.as_item_reward);
        if (!unit_item) {
            s_unit::add_supporter_unit(context, *current_user, reward_data.unit_id);
        } else {
            if (util::rng().randint(0, 1) == 1 && s_unit::has_signed_variant(context, unit_id)) unit_item->is_signed = true;

            if (current_unit_count < current_user->unit_max) {
                // Add directly
                if (!reward_data.unit_data) throw std::logic_error("unit_data is None");
                if (!reward_data.full_info) throw std::logic_error("full_info is None");

                reward_data.unit_data->is_signed = unit_item->is_signed;

                db::main::Unit *added = s_unit::add_unit_by_object(context, *current_user, *reward_data.unit_data);
                // Update unit_owning_user_id
                reward_data.update_unit_owning_user_id(*added);
                current_unit_count = current_unit_count + 1;
            } else {
                // Move to present box
                const db::unit::Unit *unit_info = s_unit::get_unit_info(context, reward_data.unit_id);
                if (!unit_info) throw std::logic_error("unit_info is None");
                unit_item->reward_box_flag = true;
                s_reward::add_item(context, *current_user, unit_item, "FIXME scouting JP Text", std::string("Scouting"),
                                 (unit_info->rarity <= 2 && unit_info->disable_rank_up == 0) ? unit_expiry : 0);
            }
        }
        unit_data_list.push_back(reward_data.as_item_reward);
        lowest_rarity = std::min(lowest_rarity, LOWEST_RARITY_SORT_ORDER[*reward_data.as_item_reward->unit_rarity_id - 1]);
    }

    // Trigger achievement
    s_achievement::AchievementContext achievement_list = s_achievement::check(
        context, *current_user,
        {s_achievement::AchievementUpdateSecretbox{secretbox_id, secretbox_button.unit_count},
         s_achievement::AchievementUpdateNewUnit{}, s_achievement::AchievementUpdateUnitRankUp{{}}});
    std::vector<std::vector<model::Ptr<item_model::Item>>> accomplished_rewards, unaccomplished_rewards;
    for (const auto *ach : achievement_list.accomplished)
        accomplished_rewards.push_back(s_achievement::get_achievement_rewards(context, *ach));
    for (const auto *ach : achievement_list.new_)
        unaccomplished_rewards.push_back(s_achievement::get_achievement_rewards(context, *ach));
    accomplished_rewards = s_advanced::fixup_achievement_reward(context, *current_user, accomplished_rewards);
    unaccomplished_rewards = s_advanced::fixup_achievement_reward(context, *current_user, unaccomplished_rewards);
    s_achievement::process_achievement_reward(context, *current_user, achievement_list.accomplished,
                                            accomplished_rewards);

    // Subtract currency
    s_secretbox::sub_user_currency(context, *current_user, secretbox_cost.cost_type, secretbox_cost.cost_item_id,
                                 cost_amount);

    if (umi_rare_mode) unit_data_list[0]->unit_rarity_id = 4;

    auto item_list = s_item::get_item_list(context, *current_user);

    SecretboxItems secret_box_items;
    secret_box_items.unit = unit_data_list;
    SecretboxPonResponse response = SecretboxPonResponse::make(
        before_user_info, s_user::get_user_info(context, *current_user),
        s_achievement::to_game_representation(context, achievement_list.accomplished, accomplished_rewards),
        s_achievement::get_achievement_count(context, *current_user, false),
        s_achievement::to_game_representation(context, achievement_list.new_, unaccomplished_rewards),
        (int64_t)achievement_list.new_.size(), current_unit_count >= current_user->unit_max, std::get<0>(item_list),
        s_secretbox::get_secretbox_button_response(context, *current_user, secretbox_data),
        s_secretbox::get_secretbox_info_response(
            context, *current_user, secretbox_data,
            s_secretbox::get_user_currency(context, *current_user, secretbox_cost.cost_type, secretbox_cost.cost_item_id) >=
                cost_amount),
        secret_box_items, secretbox_data.parcel_type, s_museum::get_museum_info_data(context, *current_user),
        s_reward::count_presentbox(context, *current_user));
    response.promotion_performance_rate = umi_rare_mode ? 100 : 10;
    response.lowest_rarity = LOWEST_RARITY_SORT_ORDER[lowest_rarity - 1];
    return response;
}
NPPS_ENDPOINT("secretbox", "pon", User, secretbox_gachapon, .batchable = false);
NPPS_ENDPOINT("secretbox", "multi", User, secretbox_gachapon, .batchable = false);

static SecretboxShowDetailResponse secretbox_showdetail(Context &context, const SecretboxShowDetailRequest &request) {
    util::log("STUB secretbox/showDetail %s", context.raw_request_data.c_str());
    // FIXME: Don't hardcode URLs
    // TODO: Make Member Filter in secretbox detail work.
    return SecretboxShowDetailResponse::make(
        "/webview.php/secretbox/detail?secretbox_id=" + std::to_string(request.secret_box_id), "/something");
}
NPPS_ENDPOINT("secretbox", "showDetail", User, secretbox_showdetail);

}  // namespace npps::game::secretbox
