// npps4/game/unit.py
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/album.hpp"
#include "system/exchange.hpp"
#include "system/item.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace npps::game::unit {

namespace su = ::npps::system::unit;
namespace sa = ::npps::system::achievement;
namespace sadv = ::npps::system::advanced;
namespace sex = ::npps::system::exchange;
namespace suser = ::npps::system::user;
using ::npps::system::unit_model::RemovableSkillInfoResponse;
using ::npps::system::unit_model::RemovableSkillOwningInfo;
using ::npps::system::unit_model::SupporterInfoResponse;
using ::npps::system::unit_model::UnitInfoData;

namespace {

model::Ptr<RemovableSkillOwningInfo> removable_skill_owning_info(Context &context, const db::main::User &user) {
    return std::make_shared<RemovableSkillInfoResponse>(su::get_removable_skill_info_request(context, user));
}

// Shared tail of the rankUp/merge/exchangePointRankUp endpoints.
struct AchievementResult {
    std::vector<db::main::Achievement *> accomplished, new_;
    std::vector<std::vector<model::Ptr<system::item_model::Item>>> accomplished_rewards, unaccomplished_rewards;
};

AchievementResult check_achievement(Context &context, db::main::User &current_user,
                                    const std::vector<sa::AchievementUpdate> &updates) {
    sa::AchievementContext achievement_list = sa::check(context, current_user, updates);
    AchievementResult result;
    result.accomplished = achievement_list.accomplished;
    result.new_ = achievement_list.new_;
    for (auto *ach : achievement_list.accomplished)
        result.accomplished_rewards.push_back(sa::get_achievement_rewards(context, *ach));
    for (auto *ach : achievement_list.new_)
        result.unaccomplished_rewards.push_back(sa::get_achievement_rewards(context, *ach));
    result.accomplished_rewards = sadv::fixup_achievement_reward(context, current_user, result.accomplished_rewards);
    result.unaccomplished_rewards = sadv::fixup_achievement_reward(context, current_user, result.unaccomplished_rewards);
    sa::process_achievement_reward(context, current_user, achievement_list.accomplished, result.accomplished_rewards);
    return result;
}

void fill_achievement(Context &context, db::main::User &current_user, const AchievementResult &ach,
                      system::achievement::AchievementMixin &response) {
    response.accomplished_achievement_list =
        sa::to_game_representation(context, ach.accomplished, ach.accomplished_rewards);
    response.unaccomplished_achievement_cnt = sa::get_achievement_count(context, current_user, false);
    response.added_achievement_list = sa::to_game_representation(context, ach.new_, ach.unaccomplished_rewards);
    response.new_achievement_cnt = (int64_t)ach.new_.size();
}

}  // namespace

static UnitAccessoryInfoResponse unit_accessoryall(Context &context) {
    // TODO
    util::log("stub: unit/accessoryAll %s", context.raw_request_data.c_str());
    UnitAccessoryInfoResponse response;
    response.accessory_list = Json::array();
    response.wearing_info = Json::array();
    response.especial_create_flag = false;
    return response;
}
NPPS_ENDPOINT("unit", "accessoryAll", User, unit_accessoryall);

static UnitDeckInfoResponse unit_deckinfo(Context &context) {
    db::main::User *current_user = suser::get_current(context);
    UnitDeckInfoResponse result;

    for (int64_t i = 1; i < 19; i++) {
        auto decklist = su::load_unit_deck(context, *current_user, i);

        if (decklist) {
            UnitDeckInfo deckinfo;
            int64_t j = 1;
            for (int64_t unit_id : std::get<1>(*decklist)) {
                if (unit_id > 0) {
                    UnitDeckPositionInfoResponse pos;
                    pos.position = j;
                    pos.unit_owning_user_id = unit_id;
                    deckinfo.unit_owning_user_ids.push_back(pos);
                }
                j++;
            }

            deckinfo.unit_deck_id = i;
            deckinfo.main_flag = current_user->active_deck_index == i;
            deckinfo.deck_name = std::get<0>(*decklist)->name;
            result.push_back(deckinfo);
        }
    }

    return result;
}
NPPS_ENDPOINT("unit", "deckInfo", User, unit_deckinfo);

static RemovableSkillInfoResponse unit_removableskillinfo(Context &context) {
    db::main::User *current_user = suser::get_current(context);
    return su::get_removable_skill_info_request(context, *current_user);
}
NPPS_ENDPOINT("unit", "removableSkillInfo", User, unit_removableskillinfo);

static system::unit_model::SupporterListInfoResponse unit_supporterall(Context &context) {
    db::main::User *current_user = suser::get_current(context);
    auto units = su::get_all_supporter_unit(context, *current_user);

    system::unit_model::SupporterListInfoResponse response;
    for (const auto &supp : units)
        response.unit_support_list.push_back(SupporterInfoResponse::make(std::get<0>(supp), std::get<1>(supp)));
    return response;
}
NPPS_ENDPOINT("unit", "supporterAll", User, unit_supporterall);

static UnitAllInfoResponse unit_unitall(Context &context) {
    db::main::User *current_user = suser::get_current(context);

    std::vector<model::Ptr<UnitInfoData>> inactive, active;

    for (db::main::Unit *unit_data : su::get_all_units(context, *current_user)) {
        auto unit_serialized_data = std::get<0>(su::get_unit_data_full_info(context, *unit_data));
        (unit_data->active ? active : inactive).push_back(std::make_shared<UnitInfoData>(std::move(unit_serialized_data)));
    }

    UnitAllInfoResponse response;
    response.active = std::move(active);
    response.waiting = std::move(inactive);
    return response;
}
NPPS_ENDPOINT("unit", "unitAll", User, unit_unitall);

static void unit_setdisplayrank(Context &context, const UnitSetDisplayRankRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    db::main::Unit *target_unit = su::get_unit(context, request.unit_owning_user_id);
    if (!target_unit || target_unit->user_id != current_user->id)
        throw error::IdolError(200, 720, "Invalid target unit");

    target_unit->display_rank = request.display_rank;
}
NPPS_ENDPOINT("unit", "setDisplayRank", User, unit_setdisplayrank);

static void unit_deckname(Context &context, const UnitDeckNameRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    auto deck_data = su::load_unit_deck(context, *current_user, request.unit_deck_id, true);
    sadv::test_name(context, request.deck_name);

    std::get<0>(*deck_data)->name = request.deck_name;
}
NPPS_ENDPOINT("unit", "deckName", User, unit_deckname);

static std::string repr(const UnitRemovableSkillRequest &r) {
    return "UnitRemovableSkillRequest(unit_owning_user_id=" + std::to_string(r.unit_owning_user_id) +
           ", unit_removable_skill_id=" + std::to_string(r.unit_removable_skill_id) + ")";
}

static void unit_removableskillequipment(Context &context, const UnitRemovableSkillEquipmentRequest &request) {
    db::main::User *current_user = suser::get_current(context);

    // TODO: Optimize
    RemovableSkillInfoResponse current_sis_info = su::get_removable_skill_info_request(context, *current_user);
    std::map<int64_t, int64_t> unequipped_sis_amount;
    for (const auto &sis : current_sis_info.owning_info)
        unequipped_sis_amount[sis.unit_removable_skill_id] = sis.total_amount - sis.equipped_amount;

    // Process "remove" list first
    if (request.remove && !request.remove->empty()) {
        for (const auto &remove_info : *request.remove) {
            db::main::Unit *unit_data = su::get_unit(context, remove_info.unit_owning_user_id);
            su::validate_unit(*current_user, unit_data);

            if (!su::detach_unit_removable_skill(context, *unit_data, remove_info.unit_removable_skill_id))
                throw error::IdolError(200, 720, "no such sis " + repr(remove_info));
            unequipped_sis_amount.at(remove_info.unit_removable_skill_id) += 1;
        }
    }

    // Process "equip" list
    if (request.equip && !request.equip->empty()) {
        for (const auto &equip_info : *request.equip) {
            if (unequipped_sis_amount.at(equip_info.unit_removable_skill_id) > 0) {
                db::main::Unit *unit_data = su::get_unit(context, equip_info.unit_owning_user_id);
                su::validate_unit(*current_user, unit_data);

                if (unit_data->active) {
                    if (!su::attach_unit_removable_skill(context, *unit_data, equip_info.unit_removable_skill_id))
                        throw error::IdolError(200, 720, "sis already exist " + repr(equip_info));
                    unequipped_sis_amount.at(equip_info.unit_removable_skill_id) -= 1;
                } else {
                    throw error::IdolError(200, 720, "unit is inactive " + std::to_string(equip_info.unit_owning_user_id));
                }
            } else {
                throw error::IdolError(200, 720, "out of SIS " + std::to_string(equip_info.unit_removable_skill_id));
            }
        }
    }
}
NPPS_ENDPOINT("unit", "removableSkillEquipment", User, unit_removableskillequipment);

static UnitWaitResponse unit_wait(Context &context, const UnitWaitOrActivateRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    int64_t waiting_count = su::count_units(context, *current_user, false);

    if ((waiting_count + (int64_t)request.unit_owning_user_ids.size()) > current_user->waiting_unit_max)
        throw error::IdolError(200, 720, "waiting unit out of range");

    for (int64_t unit_owning_user_id : request.unit_owning_user_ids) {
        db::main::Unit *unit_data = su::get_unit(context, unit_owning_user_id);
        su::validate_unit(*current_user, unit_data);

        if (unit_data->active) {
            for (int64_t removable_skill_id : su::get_unit_removable_skills(context, *unit_data))
                su::detach_unit_removable_skill(context, *unit_data, removable_skill_id);

            su::detach_from_all_deck(context, *current_user, *unit_data);
            // Move to waiting room
            unit_data->active = false;
        }
    }

    UnitWaitResponse response;
    response.unit_removable_skill = removable_skill_owning_info(context, *current_user);
    return response;
}
NPPS_ENDPOINT("unit", "wait", User, unit_wait);

static void unit_activate(Context &context, const UnitWaitOrActivateRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    int64_t active_count = su::count_units(context, *current_user, true);

    if ((active_count + (int64_t)request.unit_owning_user_ids.size()) > current_user->unit_max)
        throw error::IdolError(200, 720, "active unit out of range");

    for (int64_t unit_owning_user_id : request.unit_owning_user_ids) {
        db::main::Unit *unit_data = su::get_unit(context, unit_owning_user_id);
        su::validate_unit(*current_user, unit_data);

        // Move to active room
        unit_data->active = true;
    }
}
NPPS_ENDPOINT("unit", "activate", User, unit_activate);

static void unit_deck(Context &context, const UnitDeckRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    std::set<int64_t> clear_deck_ids;
    for (int64_t i = su::VALID_DECK_ID_FIRST; i <= su::VALID_DECK_ID_LAST; i++) clear_deck_ids.insert(i);
    static const std::set<int64_t> ALL_DECK_POSITIONS = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Replace/add deck
    for (const auto &deck_req_info : request.unit_deck_list) {
        sadv::test_name(context, deck_req_info.deck_name);

        db::main::UnitDeck *unit_deck_row =
            std::get<0>(*su::load_unit_deck(context, *current_user, deck_req_info.unit_deck_id, true));
        unit_deck_row->name = deck_req_info.deck_name;

        // Replace decks
        std::vector<int64_t> unit_user_ids(9, 0);
        for (const auto &detail : deck_req_info.unit_deck_detail) {
            // Unit sanity check
            db::main::Unit *unit_data = su::get_unit(context, detail.unit_owning_user_id);
            su::validate_unit(*current_user, unit_data);
            int64_t idx = detail.position - 1;  // Python list indexing, negative wraps around
            if (idx < 0) idx += 9;
            if (idx < 0 || idx >= 9) throw std::out_of_range("list assignment index out of range");
            unit_user_ids[(size_t)idx] = detail.unit_owning_user_id;
        }

        if (deck_req_info.main_flag > 0) {
            // Perform additional sanity check
            std::set<int64_t> registered_deck_positions;
            for (const auto &detail : deck_req_info.unit_deck_detail)
                if (detail.unit_owning_user_id > 0) registered_deck_positions.insert(detail.position);

            if (registered_deck_positions != ALL_DECK_POSITIONS) throw error::by_code(error::ERROR_CODE_IGNORE_MAIN_DECK);

            current_user->active_deck_index = deck_req_info.unit_deck_id;
        }

        su::save_unit_deck(context, *current_user, *unit_deck_row, unit_user_ids);
        if (!clear_deck_ids.erase(deck_req_info.unit_deck_id)) throw std::out_of_range("KeyError");
    }

    // Clear deck
    for (int64_t deck_id : clear_deck_ids) {
        auto unit_deck_data = su::load_unit_deck(context, *current_user, deck_id, false);
        if (unit_deck_data) su::save_unit_deck(context, *current_user, *std::get<0>(*unit_deck_data), std::vector<int64_t>(9, 0));
    }
}
NPPS_ENDPOINT("unit", "deck", User, unit_deck);

static UnitRankUpResponse unit_rankup(Context &context, const UnitRankUpRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    db::main::Unit *source_unit = su::get_unit(context, request.base_owning_unit_user_id);
    su::validate_unit(*current_user, source_unit);

    // Get needed data
    const db::unit::Unit *source_unit_info = su::get_unit_info(context, source_unit->unit_id);
    if (source_unit_info->disable_rank_up != (int64_t)const_::UNIT_CATEGORY::NORMAL)
        throw error::IdolError(200, 720, "cannot idolize this unit");

    const db::unit::Rarity *source_unit_rarity = su::get_unit_rarity(context, source_unit_info->rarity);
    if (!source_unit_rarity) throw std::logic_error("assertion failed");

    auto before = su::get_unit_data_full_info(context, *source_unit);
    UnitRankUpResponse response;
    response.before_user_info = suser::get_user_info(context, *current_user);
    int64_t use_game_coin = 0;

    for (int64_t unit_owning_user_id : request.unit_owning_user_ids) {
        if (source_unit->rank >= source_unit_info->rank_max &&
            source_unit->unit_removable_skill_capacity >= source_unit_info->max_removable_skill_capacity)
            throw error::IdolError(200, 720, "max SIS reached");

        db::main::Unit *target_unit = su::get_unit(context, unit_owning_user_id);
        su::remove_unit(context, *current_user, target_unit);
        int64_t add_removable_skill_slot = 2 - (int64_t)su::idolize(context, *current_user, *source_unit);
        source_unit->unit_removable_skill_capacity =
            std::min(source_unit->unit_removable_skill_capacity + add_removable_skill_slot,
                     source_unit_info->max_removable_skill_capacity);
        use_game_coin = use_game_coin + source_unit_rarity->rank_up_cost;

        if (current_user->game_coin >= source_unit_rarity->rank_up_cost)
            current_user->game_coin = current_user->game_coin - source_unit_rarity->rank_up_cost;
        else
            throw error::IdolError(200, 720, "not enough money");
    }

    system::album::update(context, *current_user, source_unit->unit_id, true);
    AchievementResult ach = check_achievement(
        context, *current_user, {sa::AchievementUpdateUnitRankUp{std::vector<int64_t>{source_unit->unit_id}}});

    auto after = su::get_unit_data_full_info(context, *source_unit);
    response.after_user_info = suser::get_user_info(context, *current_user);

    fill_achievement(context, *current_user, ach, response);
    response.before = std::make_shared<UnitInfoData>(std::get<0>(before));
    response.after = std::make_shared<UnitInfoData>(std::get<0>(after));
    response.use_game_coin = use_game_coin;
    response.unlocked_subscenario_ids = {};
    response.get_exchange_point_list = {};
    response.unit_removable_skill = removable_skill_owning_info(context, *current_user);
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("unit", "rankUp", User, unit_rankup, .batchable = false);

static UnitSaleResponse unit_sale(Context &context, const UnitSaleRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    int64_t total = 0;
    std::vector<UnitSaleDetail> detail;
    std::vector<std::pair<int64_t, int64_t>> exchange_point;
    UnitSaleResponse response;
    response.before_user_info = suser::get_user_info(context, *current_user);

    // Remove regular unit
    for (int64_t unit_owning_user_id : request.unit_owning_user_id) {
        db::main::Unit *unit_data = su::get_unit(context, unit_owning_user_id);
        su::validate_unit(*current_user, unit_data);

        // Get sale price
        su::UnitStatsResult unit_stats = std::get<1>(su::get_unit_data_full_info(context, *unit_data));
        UnitSaleDetail d;
        d.unit_owning_user_id = unit_owning_user_id;
        d.unit_id = unit_data->unit_id;
        d.is_signed = unit_data->is_signed;
        d.amount = 1;
        d.price = unit_stats.sale_price;
        detail.push_back(d);
        total = total + unit_stats.sale_price;
        su::remove_unit(context, *current_user, unit_data);

        // Give sticker
        if (sex::should_give_sticker(context, unit_data->unit_id)) {
            int64_t exchange_point_id = su::get_exchange_point_id_by_unit_id(context, unit_data->unit_id);
            if (exchange_point_id > 0) {
                auto it = std::find_if(exchange_point.begin(), exchange_point.end(),
                                       [&](const auto &e) { return e.first == exchange_point_id; });
                if (it == exchange_point.end()) exchange_point.emplace_back(exchange_point_id, 1);
                else it->second += 1;
            }
        }
    }

    // Remove support unit
    for (const auto &supp_unit : request.unit_support_list) {
        if (supp_unit.amount > 0) {
            const db::unit::Unit *unit_info = su::get_unit_info(context, supp_unit.unit_id);

            if (su::sub_supporter_unit(context, *current_user, supp_unit.unit_id)) {
                const auto &unit_level_up_pattern = su::get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
                const auto *unit_stats = unit_level_up_pattern.at(0);
                UnitSaleDetail d;
                d.unit_owning_user_id = 0;
                d.unit_id = supp_unit.unit_id;
                d.is_signed = false;
                d.amount = supp_unit.amount;
                d.price = unit_stats->sale_price * supp_unit.amount;
                detail.push_back(d);
                total = total + unit_stats->sale_price * supp_unit.amount;
            } else {
                throw error::IdolError(200, 720, "invalid unit amount");
            }
        }
    }

    // Give stickers
    std::vector<system::exchange::ExchangePointInfo> get_exchange_point_list;
    for (const auto &[exchange_point_id, amount] : exchange_point) {
        sex::add_exchange_point(context, *current_user, exchange_point_id, amount);
        get_exchange_point_list.push_back(system::exchange::ExchangePointInfo::make(exchange_point_id, amount));
    }

    // Give coin
    bool reward_box_flag = !static_cast<bool>(sadv::add_item(context, *current_user, system::item::game_coin(total)));

    response.after_user_info = suser::get_user_info(context, *current_user);
    response.total = total;
    response.detail = std::move(detail);
    response.reward_box_flag = reward_box_flag;
    response.get_exchange_point_list = std::move(get_exchange_point_list);
    response.unit_removable_skill = removable_skill_owning_info(context, *current_user);
    return response;
}
NPPS_ENDPOINT("unit", "sale", User, unit_sale, .batchable = false);

static UnitMergeResponse unit_merge(Context &context, const UnitMergeRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    UnitMergeResponse response;
    response.before_user_info = suser::get_user_info(context, *current_user);
    db::main::Unit *source_unit = su::get_unit(context, request.base_owning_unit_user_id);
    su::validate_unit(*current_user, source_unit);

    UnitInfoData before_unit = std::get<0>(su::get_unit_data_full_info(context, *source_unit));
    const db::unit::Unit *source_unit_info = su::get_unit_info(context, source_unit->unit_id);
    int64_t source_unit_attribute = source_unit_info->attribute_id;
    std::optional<int64_t> source_unit_skill = source_unit_info->default_unit_skill_id;

    // Calculate total EXP and the cost
    std::vector<std::pair<int64_t, int64_t>> exchange_point;
    int64_t total_exp = 0;
    int64_t merge_cost = 0;
    int64_t skill_exp = 0;
    int64_t member_count = 0;
    for (int64_t unit_owning_user_id : request.unit_owning_user_ids) {
        db::main::Unit *unit_data = su::get_unit(context, unit_owning_user_id);
        su::validate_unit(*current_user, unit_data);
        const db::unit::Unit *unit_info = su::get_unit_info(context, unit_data->unit_id);
        auto [unit_full_info, unit_stats] = su::get_unit_data_full_info(context, *unit_data);
        double multipler = (source_unit_attribute == unit_info->attribute_id) * 0.2;
        merge_cost = merge_cost + unit_stats.merge_cost;
        // Note: Flooring is correct behavior.
        total_exp = total_exp + (int64_t)std::floor((double)unit_stats.merge_exp * (1 + multipler));
        su::remove_unit(context, *current_user, unit_data);

        // Give skill EXP
        if (source_unit_skill && source_unit_skill == unit_info->default_unit_skill_id) {
            const db::unit::UnitSkillLevel *skill_level_data =
                su::get_unit_skill_level_data(context, *source_unit_skill, unit_full_info.unit_skill_level);
            if (skill_level_data) skill_exp = skill_exp + skill_level_data->grant_exp;
        }

        // Give sticker
        if (sex::should_give_sticker(context, unit_data->unit_id)) {
            int64_t exchange_point_id = su::get_exchange_point_id_by_unit_id(context, unit_data->unit_id);
            if (exchange_point_id > 0) {
                auto it = std::find_if(exchange_point.begin(), exchange_point.end(),
                                       [&](const auto &e) { return e.first == exchange_point_id; });
                if (it == exchange_point.end()) exchange_point.emplace_back(exchange_point_id, 1);
                else it->second += 1;
            }
        }

        member_count = member_count + 1;
    }

    for (const auto &supp_unit : request.unit_support_list) {
        if (supp_unit.amount > 0) {
            const db::unit::Unit *unit_info = su::get_unit_info(context, supp_unit.unit_id);

            if (su::sub_supporter_unit(context, *current_user, supp_unit.unit_id, supp_unit.amount)) {
                const auto &unit_level_up_pattern = su::get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
                const auto *unit_stats = unit_level_up_pattern.at(0);
                double multipler = (source_unit_attribute == unit_info->attribute_id) * 0.2;
                merge_cost = merge_cost + unit_stats->merge_cost * supp_unit.amount;
                total_exp = total_exp + (int64_t)std::floor((double)unit_stats->merge_exp * (1 + multipler)) * supp_unit.amount;

                // Give skill EXP
                if (unit_info->default_unit_skill_id) {
                    const db::unit::UnitSkillLevel *skill_level_data =
                        su::get_unit_skill_level_data(context, *unit_info->default_unit_skill_id, 1);
                    if (skill_level_data) skill_exp = skill_exp + skill_level_data->grant_exp * supp_unit.amount;
                }
            } else {
                throw error::IdolError(200, 720, "invalid unit amount");
            }

            member_count = member_count + supp_unit.amount;
        }
    }

    if (merge_cost > current_user->game_coin) throw error::IdolError(200, 720, "not enough game coin");

    // Determine evolution type
    const_::EVOLUTION_BONUS_TYPE evolution_type = util::rng().choices(
        std::vector<const_::EVOLUTION_BONUS_TYPE>{const_::EVOLUTION_BONUS_TYPE::NORMAL,
                                                  const_::EVOLUTION_BONUS_TYPE::SUPER_SUCCESS,
                                                  const_::EVOLUTION_BONUS_TYPE::ULTRA_SUCCESS},
        std::vector<int64_t>{94, 5, 1}, 1)[0];
    double evolution_bonus = su::get_exp_multiplier(evolution_type);
    // FIXME: Rectify if it rounds down or up
    total_exp = (int64_t)std::ceil((double)total_exp * evolution_bonus);

    // Get unit max EXP and skill EXP
    int64_t max_exp = su::get_max_exp(context, *source_unit, source_unit_info);
    int64_t max_skill_exp = su::get_max_skill_exp(context, *source_unit_info);

    // Update unit
    source_unit->exp = std::min(source_unit->exp + total_exp, max_exp);
    source_unit->skill_exp = std::min(source_unit->skill_exp + skill_exp, max_skill_exp);

    // Subtract game coin
    current_user->game_coin = current_user->game_coin - merge_cost;

    // Give stickers
    std::vector<system::exchange::ExchangePointInfo> get_exchange_point_list;
    for (const auto &[exchange_point_id, amount] : exchange_point) {
        sex::add_exchange_point(context, *current_user, exchange_point_id, amount);
        get_exchange_point_list.push_back(system::exchange::ExchangePointInfo::make(exchange_point_id, amount));
    }

    // Update album
    UnitInfoData after_unit = std::get<0>(su::get_unit_data_full_info(context, *source_unit));
    system::album::update(context, *current_user, source_unit->unit_id, false, false, after_unit.is_level_max);

    // Trigger achievement
    std::vector<sa::AchievementUpdate> ach_list_to_test;
    ach_list_to_test.push_back(sa::AchievementUpdateUnitRankUp{});
    ach_list_to_test.push_back(sa::AchievementUpdateUnitMerge{source_unit->id, after_unit.unit_skill_level, member_count});
    if (after_unit.is_level_max) ach_list_to_test.push_back(sa::AchievementUpdateUnitMaxLevel{});
    AchievementResult ach = check_achievement(context, *current_user, ach_list_to_test);

    response.after_user_info = suser::get_user_info(context, *current_user);
    fill_achievement(context, *current_user, ach, response);
    response.before = std::make_shared<UnitInfoData>(before_unit);
    response.after = std::make_shared<UnitInfoData>(after_unit);
    response.use_game_coin = merge_cost;
    response.evolution_bonus_type = evolution_type;
    response.bonus_value = evolution_bonus;
    response.get_exchange_point_list = std::move(get_exchange_point_list);
    response.unit_removable_skill = removable_skill_owning_info(context, *current_user);
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("unit", "merge", User, unit_merge, .batchable = false);

static UnitExchangeRankUpResponse unit_exchangepointrankup(Context &context,
                                                           const UnitExchangePointRankUpRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    db::main::Unit *source_unit = su::get_unit(context, request.base_owning_unit_user_id);
    su::validate_unit(*current_user, source_unit);

    // Get needed data
    const db::unit::Unit *source_unit_info = su::get_unit_info(context, source_unit->unit_id);
    if (source_unit_info->disable_rank_up > 0) throw error::IdolError(200, 720, "cannot idolize this unit");

    const db::unit::Rarity *source_unit_rarity = su::get_unit_rarity(context, source_unit_info->rarity);
    if (!source_unit_rarity) throw std::logic_error("assertion failed");

    if (current_user->game_coin < source_unit_rarity->exchange_point_rank_up_cost)
        throw error::IdolError(200, 720, "not enough game coin");

    // Check sticker eglibility
    int64_t need_amount = sex::get_exchange_needed_to_idolize(context, request.exchange_point_id, source_unit_info->rarity);
    if (need_amount == 0) {
        // Can can but can't
        throw error::IdolError(200, 720, "cannot idolize with this sticker-rarity combination");
    }

    auto before = su::get_unit_data_full_info(context, *source_unit);
    UnitExchangeRankUpResponse response;
    response.before_user_info = suser::get_user_info(context, *current_user);

    if (!sex::sub_exchange_point(context, *current_user, request.exchange_point_id, need_amount))
        throw error::IdolError(200, 720, "not enough sticker");

    // It's paid, now hit it.
    if (source_unit->rank < source_unit_info->rank_max) {
        // Idolize
        su::idolize(context, *current_user, *source_unit);
    } else if (source_unit->unit_removable_skill_capacity < source_unit_info->max_removable_skill_capacity) {
        // Unlock SIS slot
        source_unit->unit_removable_skill_capacity = source_unit->unit_removable_skill_capacity + 1;
    } else {
        throw error::IdolError(200, 720, "max SIS reached");
    }

    system::album::update(context, *current_user, source_unit->unit_id, true);
    AchievementResult ach = check_achievement(
        context, *current_user, {sa::AchievementUpdateUnitRankUp{std::vector<int64_t>{source_unit->unit_id}}});

    // Subtract game coin
    current_user->game_coin = current_user->game_coin - source_unit_rarity->exchange_point_rank_up_cost;

    auto after = su::get_unit_data_full_info(context, *source_unit);
    response.after_user_info = suser::get_user_info(context, *current_user);

    fill_achievement(context, *current_user, ach, response);
    response.before = std::make_shared<UnitInfoData>(std::get<0>(before));
    response.after = std::make_shared<UnitInfoData>(std::get<0>(after));
    response.after_exchange_point = sex::get_exchange_point_amount(context, *current_user, request.exchange_point_id);
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("unit", "exchangePointRankUp", User, unit_exchangepointrankup, .batchable = false);

static void unit_favorite(Context &context, const UnitFavoriteRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    db::main::Unit *source_unit = su::get_unit(context, request.unit_owning_user_id);
    su::validate_unit(*current_user, source_unit);
    source_unit->favorite_flag = request.favorite_flag > 0;
}
NPPS_ENDPOINT("unit", "favorite", User, unit_favorite);

static UnitRemovableSkillSellResponse unit_removableskillsell(Context &context,
                                                              const UnitRemovableSkillSellRequest &request) {
    db::main::User *current_user = suser::get_current(context);
    int64_t total = 0;

    for (const auto &info : request.selling_list) {
        const db::unit::RemovableSkill *sis = su::get_removable_skill_game_info(context, info.unit_removable_skill_id);
        if (sis) {
            // FIXME: Check if amount is sensible across units and their assigned SIS
            int64_t new_amount = su::sub_unit_removable_skill(context, *current_user, info.unit_removable_skill_id, info.amount);
            if (new_amount < 0)
                throw error::IdolError(200, 720,
                                       "cannot sell SIS " + std::to_string(info.unit_removable_skill_id) + " amount " +
                                           std::to_string(-new_amount));

            total = sis->selling_price * info.amount;
        }
    }

    bool success_add = static_cast<bool>(sadv::add_item(context, *current_user, system::item::game_coin(total)));
    UnitRemovableSkillSellResponse response;
    response.after_user_info = suser::get_user_info(context, *current_user);
    response.total = total;
    response.reward_box_flag = !success_add;
    return response;
}
NPPS_ENDPOINT("unit", "removableSkillSell", User, unit_removableskillsell);

}  // namespace npps::game::unit
