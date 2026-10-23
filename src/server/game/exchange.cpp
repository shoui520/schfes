// npps4/game/exchange.py
#include "app/idol.hpp"
#include "system/advanced.hpp"
#include "system/exchange.hpp"
#include "system/reward.hpp"
#include "system/user.hpp"

#include <algorithm>
#include <array>

namespace npps::game::exchange {

static constexpr std::array<const_::ADD_TYPE, 16> STACKABLE_ITEMS = {
    const_::ADD_TYPE::ITEM,
    const_::ADD_TYPE::GAME_COIN,
    const_::ADD_TYPE::LOVECA,
    const_::ADD_TYPE::SOCIAL_POINT,
    const_::ADD_TYPE::INVITE_POINT,
    const_::ADD_TYPE::PLAYER_EXP,
    const_::ADD_TYPE::UNIT_MAX,
    const_::ADD_TYPE::EXCHANGE_POINT,
    const_::ADD_TYPE::FRIEND_MAX,
    const_::ADD_TYPE::WAITING_UNIT_MAX,
    const_::ADD_TYPE::TRAINING_MAX,
    const_::ADD_TYPE::SCHOOL_IDOL_SKILL,
    const_::ADD_TYPE::EVENT_POINT,
    const_::ADD_TYPE::LOTTERY_TICKET,
    const_::ADD_TYPE::RECOVER_LP_ITEM,
    const_::ADD_TYPE::TRADE_CAPITAL,
};

static ExchangePointResponse exchange_owningpoint(Context &context) {
    return ExchangePointResponse::make(
        system::exchange::get_exchange_points_response(context, *system::user::get_current(context)));
}
NPPS_ENDPOINT("exchange", "owningPoint", User, exchange_owningpoint);

static ExchangeItemInfoResponse exchange_iteminfo(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<system::exchange::ExchangePointInfo> points =
        system::exchange::get_exchange_points_response(context, *system::user::get_current(context));
    return ExchangeItemInfoResponse::make(points, system::exchange::get_exchange_item_info(context, *current_user));
}
NPPS_ENDPOINT("exchange", "itemInfo", User, exchange_iteminfo);

static ExchangeUsePointResponse exchange_usepoint(Context &context, const ExchangeUsePointRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    const data::schema::StickerShop *raw_exchange_info =
        system::exchange::find_raw_exchange_item_info_by_id(context, request.exchange_item_id);
    int64_t time = util::time();

    // Checks
    if (request.amount <= 0) throw error::IdolError(200, 720, "Invalid amount");
    if (raw_exchange_info == nullptr) throw error::by_code(error::ERROR_CODE_EXCHANGE_INVALID);
    if (raw_exchange_info->end_time != 0 && time > raw_exchange_info->end_time)
        throw error::by_code(error::ERROR_CODE_EXCHANGE_ITEM_OUT_OF_DATE);

    model::Ptr<system::exchange::ExchangeItemBase> exchange_info =
        system::exchange::get_exchange_item_info_by_raw_info(context, *current_user, *raw_exchange_info, time);
    if (auto with_max = std::dynamic_pointer_cast<system::exchange::ExchangeItemWithMaxCount>(exchange_info)) {
        if (with_max->got_item_count + request.amount > with_max->max_item_count)
            throw error::by_code(error::ERROR_CODE_OVER_ADD_EXCHANGE_ITEM_COUNT_MAX_LIMIT);
    } else if (auto with_max_expiry =
                   std::dynamic_pointer_cast<system::exchange::ExchangeItemWithMaxCountAndExpiry>(exchange_info)) {
        if (with_max_expiry->got_item_count + request.amount > with_max_expiry->max_item_count)
            throw error::by_code(error::ERROR_CODE_OVER_ADD_EXCHANGE_ITEM_COUNT_MAX_LIMIT);
    }

    const data::schema::ExchangeCost *exchange_rarity = nullptr;
    for (const data::schema::ExchangeCost &rarity : raw_exchange_info->costs) {
        if (rarity.rarity == request.rarity) {
            exchange_rarity = &rarity;
            break;
        }
    }
    if (exchange_rarity == nullptr) throw error::by_code(error::ERROR_CODE_NOT_ENOUGH_EXCHANGE_POINT);

    int64_t required_exchange_point = exchange_rarity->cost * request.amount;
    int64_t current_exchange_point =
        system::exchange::get_exchange_point_amount(context, *current_user, request.rarity);
    if (current_exchange_point < required_exchange_point)
        throw error::by_code(error::ERROR_CODE_NOT_ENOUGH_EXCHANGE_POINT);

    // Specific case for background and award
    if (raw_exchange_info->add_type == const_::ADD_TYPE::AWARD ||
        raw_exchange_info->add_type == const_::ADD_TYPE::BACKGROUND) {
        if (system::reward::has_at_least_one(context, *current_user, raw_exchange_info->add_type,
                                             raw_exchange_info->item_id)) {
            int errcode = raw_exchange_info->add_type == const_::ADD_TYPE::AWARD
                              ? error::ERROR_CODE_AWARD_IS_CONTAINED_IN_BOX
                              : error::ERROR_CODE_BACKGROUND_IS_CONTAINED_IN_BOX;
            throw error::by_code(errcode);
        }
    }

    // Capture old user info
    system::user::UserInfoData before_user_info = system::user::get_user_info(context, *current_user);

    // Perform the exchange
    std::vector<model::Ptr<system::item_model::Item>> item_result;
    if (std::find(STACKABLE_ITEMS.begin(), STACKABLE_ITEMS.end(), raw_exchange_info->add_type) !=
        STACKABLE_ITEMS.end()) {
        system::item_model::BaseItem item_data_copy;
        item_data_copy.add_type = raw_exchange_info->add_type;
        item_data_copy.item_id = raw_exchange_info->item_id;
        item_data_copy.amount = raw_exchange_info->amount * request.amount;
        item_data_copy.extra_data = raw_exchange_info->extra_data;
        model::Ptr<system::item_model::Item> item_data = system::advanced::deserialize_item_data(context, item_data_copy);
        item_data->reward_box_flag = true;
        system::reward::add_item(context, *current_user, item_data, "Sticker Shop");
        item_result.push_back(item_data);
    } else {
        for (int64_t i = 0; i < request.amount; i++) {
            model::Ptr<system::item_model::Item> item_data =
                system::advanced::deserialize_item_data(context, *raw_exchange_info);
            item_data->reward_box_flag = true;
            system::reward::add_item(context, *current_user, item_data, "Sticker Shop");
            item_result.push_back(item_data);
        }
    }

    system::exchange::sub_exchange_point(context, *current_user, exchange_rarity->rarity, required_exchange_point);
    system::exchange::add_exchange_item_got_count(context, *current_user, exchange_info->exchange_item_id,
                                                  request.amount);

    ExchangeUsePointResponse response;
    response.before_user_info = before_user_info;
    response.after_user_info = system::user::get_user_info(context, *current_user);
    response.exchange_reward = item_result;
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("exchange", "usePoint", User, exchange_usepoint);

}  // namespace npps::game::exchange
