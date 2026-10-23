// npps4/game/live.py
#include "app/config.hpp"
#include "app/idol.hpp"
#include "external/live_unit_drop.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/const.hpp"
#include "system/effort.hpp"
#include "system/item.hpp"
#include "system/live.hpp"
#include "system/museum.hpp"
#include "system/ranking.hpp"
#include "system/reward.hpp"
#include "system/scenario.hpp"
#include "system/subscenario.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace npps::game::live {

namespace sys = ::npps::system;
using sys::live_model::LiveInfo;

static constexpr bool DEBUG_SERVER_SCORE_CALCULATE = false;

// Python truthiness of a JSON value.
static bool truthy(const Json &j) {
    switch (j.type()) {
    case Json::Null: return false;
    case Json::Bool: return j.as_bool();
    case Json::Int: return j.as_int() != 0;
    case Json::Double: return j.as_double() != 0.0;
    case Json::String: return !j.as_string().empty();
    default: return j.size() != 0;
    }
}

static std::vector<Json> dump_items_list(const std::vector<model::Ptr<sys::item_model::Item>> &items) {
    return model::dump(items).elements();
}

static LiveRewardResponseUnitList unit_list_from_unit_data(const sys::unit_model::UnitInfoData &unit_data_full,
                                                           int64_t position, int64_t before_love) {
    LiveRewardResponseUnitList r;
    r.unit_owning_user_id = unit_data_full.unit_owning_user_id;
    r.unit_id = unit_data_full.unit_id;
    r.position = position;
    r.level = unit_data_full.level;
    r.level_limit_id = unit_data_full.level_limit_id;
    r.before_love = before_love;
    r.love = unit_data_full.love;
    r.max_love = unit_data_full.max_love;
    r.unit_skill_level = unit_data_full.unit_skill_level;
    r.display_rank = unit_data_full.display_rank;
    r.is_rank_max = unit_data_full.is_rank_max;
    r.is_love_max = unit_data_full.is_love_max;
    r.is_level_max = unit_data_full.is_level_max;
    r.is_signed = unit_data_full.is_signed;
    return r;
}

static std::vector<LivePlayRankInfo> make_rank_info(const db::live::LiveSetting &live_setting) {
    return {
        LivePlayRankInfo::make(5, 0, live_setting.c_rank_score - 1),
        LivePlayRankInfo::make(4, live_setting.c_rank_score, live_setting.b_rank_score - 1),
        LivePlayRankInfo::make(3, live_setting.b_rank_score, live_setting.a_rank_score - 1),
        LivePlayRankInfo::make(2, live_setting.a_rank_score, live_setting.s_rank_score - 1),
        LivePlayRankInfo::make(1, live_setting.s_rank_score, 0),
    };
}

// Checks the LP (or throws) and returns the LP cost.
static int64_t check_live_lp(const sys::live::CommonLive &live_info, db::main::User &current_user,
                             const LivePartyListRequest *, int64_t lp_factor) {
    int64_t cap_value = (int64_t)std::ceil((double)(live_info.capital_value * lp_factor) * config::get().energy_multiplier);
    switch (live_info.capital_type) {
    case 1:
        if (!sys::user::has_energy(current_user, cap_value)) {
            if (current_user.energy_max < cap_value) throw error::by_code(error::ERROR_CODE_LIVE_NOT_ENOUGH_MAX_ENERGY);
            else throw error::by_code(error::ERROR_CODE_LIVE_NOT_ENOUGH_CURRENT_ENERGY);
        }
        break;
    default:
        // TODO
        throw error::by_code(error::ERROR_CODE_LIVE_NOT_ENOUGH_EVENT_POINT);
    }
    return cap_value;
}

static LiveStatusResponse live_livestatus(Context &context) {
    db::main::User *current_user = sys::user::get_current(context);
    LiveStatusResponse r;
    r.normal_live_status_list = sys::live::get_normal_live_clear_status(context, *current_user);
    r.special_live_status_list = sys::live::get_special_live_status(context, *current_user);
    r.training_live_status_list = sys::live::get_training_live_status(context, *current_user);
    r.marathon_live_status_list = Json::array();
    r.free_live_status_list = Json::array();
    r.can_resume_live = true;
    return r;
}
NPPS_ENDPOINT("live", "liveStatus", User, live_livestatus);

static LiveScheduleResponse live_schedule(Context &context) {
    int64_t ts = util::time();
    auto special_live_rotation = sys::live::get_special_live_rotation_difficulty_id(context);

    LiveScheduleResponse r;
    for (const auto &entry : special_live_rotation) {
        LimitedLive l;
        l.live_difficulty_id = entry.second;
        l.start_date = util::timestamp_to_datetime(util::get_next_day_timestamp(ts, 0));
        l.end_date = util::timestamp_to_datetime(util::get_next_day_timestamp(ts));
        l.is_random = false;  // TODO
        r.live_list.push_back(std::move(l));
    }
    r.limited_bonus_list = Json::array();
    for (int64_t i = 1; i < 4; i++)
        r.random_live_list.push_back(
            RandomLive::make(i, util::timestamp_to_datetime(0), util::timestamp_to_datetime(2147483647)));
    r.free_live_list = Json::array();
    return r;
}
NPPS_ENDPOINT("live", "schedule", User, live_schedule);

static LivePartyListResponse live_partylist(Context &context, const LivePartyListRequest &request) {
    db::main::User *current_user = sys::user::get_current(context);

    // Check live info
    const sys::live::CommonLive *live_info = sys::live::get_live_info_table(context, request.live_difficulty_id);
    if (live_info == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);

    // Check LP
    check_live_lp(*live_info, *current_user, &request, request.lp_factor);

    const db::live::LiveSetting *live_setting = sys::live::get_live_setting(context, live_info->live_setting_id);
    if (live_setting == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);

    auto beatmap_data = sys::live::get_live_info(context, request.live_difficulty_id, *live_setting);
    if (!beatmap_data) throw error::by_code(error::ERROR_CODE_LIVE_NOTES_LIST_NOT_FOUND);

    std::vector<sys::advanced::PartyInfo> party_list;
    for (db::main::User *u : sys::advanced::get_random_user_for_partylist(context, *current_user))
        party_list.push_back(sys::advanced::get_user_guest_party_info(context, *u));

    // DEBUG live score (disabled in NPPS4, not ported)
    static_assert(!DEBUG_SERVER_SCORE_CALCULATE);

    // TODO
    return LivePartyListResponse::make(std::move(party_list), current_user->training_energy,
                                       current_user->training_energy_max);
}
NPPS_ENDPOINT("live", "partyList", User, live_partylist);

// Fills the "on"/"off" record of live/preciseScore.
static Json precise_score_dict(const LiveInfo &live_info,
                               const std::optional<std::tuple<Json, std::vector<sys::live_model::LiveNote>, int64_t>> &record) {
    if (!record)
        return Json::obj({{"has_record", false}, {"can_replay", false}, {"live_info", live_info.to_json()}});

    Json live_info_json = live_info.to_json();
    live_info_json["notes_list"] = sys::live::notes_list_dump(std::get<1>(*record));
    Json dict = std::get<0>(*record);
    dict["has_record"] = true;
    dict["can_replay"] = true;
    dict["live_info"] = std::move(live_info_json);
    dict["update_date"] = util::timestamp_to_datetime(std::get<2>(*record));
    return dict;
}

static LivePreciseScoreResponse live_precisescore(Context &context, const LivePreciseScoreRequest &request) {
    db::main::User *current_user = sys::user::get_current(context);

    const db::live::LiveSetting *live_setting =
        sys::live::get_live_setting_from_difficulty_id(context, request.live_difficulty_id);
    if (live_setting == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);
    LiveInfo live_info = sys::live::get_live_info_without_notes(context, request.live_difficulty_id, *live_setting);

    auto with_skill_record =
        sys::live::pull_precise_score_with_beatmap(context, *current_user, request.live_difficulty_id, true);
    auto without_skill_record =
        sys::live::pull_precise_score_with_beatmap(context, *current_user, request.live_difficulty_id, false);

    if (!with_skill_record && !without_skill_record)
        throw error::by_code(error::ERROR_CODE_LIVE_PRECISE_LIST_NOT_FOUND);

    Json on_dict = precise_score_dict(live_info, with_skill_record);
    Json off_dict = precise_score_dict(live_info, without_skill_record);

    auto response = LivePreciseScoreResponse::make(make_rank_info(*live_setting), std::move(on_dict), std::move(off_dict));
    response.can_activate_effect = true;  // TODO
    return response;
}
NPPS_ENDPOINT("live", "preciseScore", User, live_precisescore);

static LivePlayResponse live_play(Context &context, const LivePlayRequest &request) {
    db::main::User *current_user = sys::user::get_current(context);

    // Check LP
    const sys::live::CommonLive *live_info = sys::live::get_live_info_table(context, request.live_difficulty_id);
    if (live_info == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);

    // Check LP
    int64_t cap_value = check_live_lp(*live_info, *current_user, nullptr, request.lp_factor);

    // Consume LP
    sys::user::sub_energy(*current_user, cap_value);

    const db::live::LiveSetting *live_setting =
        sys::live::get_live_setting_from_difficulty_id(context, request.live_difficulty_id);
    if (live_setting == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);

    auto beatmap_data = sys::live::get_live_info(context, request.live_difficulty_id, *live_setting);
    if (!beatmap_data) throw error::by_code(error::ERROR_CODE_LIVE_NOTES_LIST_NOT_FOUND);

    auto deck_data = sys::unit::load_unit_deck(context, *current_user, request.unit_deck_id);
    if (!deck_data || std::count(std::get<1>(*deck_data).begin(), std::get<1>(*deck_data).end(), 0) > 0)
        throw error::IdolError(error::ERROR_CODE_LIVE_INVALID_UNIT_DECK);

    db::main::User *guest = sys::user::get(context, request.party_user_id);
    if (guest == nullptr) throw error::IdolError(error::ERROR_CODE_LIVE_INVALID_PARTY_USER);

    int64_t guest_center_unit_owning_user_id = sys::unit::get_unit_center(context, *guest, true);
    if (guest_center_unit_owning_user_id == 0) throw error::IdolError(error::ERROR_CODE_LIVE_INVALID_PARTY_USER);

    sys::advanced::TeamStatCalculator calculator(context);
    sys::museum::MuseumInfoData museum_data = sys::museum::get_museum_info_data(context, *current_user);
    std::vector<db::main::Unit *> deck_units;
    for (int64_t i : std::get<1>(*deck_data)) deck_units.push_back(sys::unit::get_unit(context, i));
    sys::advanced::LiveDeckInfo stats = calculator.get_live_stats(
        request.unit_deck_id, deck_units, *sys::unit::get_unit(context, guest_center_unit_owning_user_id),
        museum_data.parameter);

    // Register live in progress
    sys::live::register_live_in_progress(context, *current_user, *guest, request.lp_factor, request.unit_deck_id,
                                         sys::live::compact_json_dump(stats.to_json()));

    LivePlayResponse response;
    response.rank_info = make_rank_info(*live_setting);
    response.energy_full_time = util::timestamp_to_datetime(current_user->energy_full_time);
    response.over_max_energy = current_user->over_max_energy;
    // TODO: Medley Festival
    response.live_list.push_back(LivePlayList::make(std::move(*beatmap_data), std::move(stats)));
    return response;
}
NPPS_ENDPOINT("live", "play", User, live_play, .xmc_verify = XMCVerifyMode::CROSS);

static void live_gameover(Context &context) {
    db::main::User *current_user = sys::user::get_current(context);
    sys::live::clean_live_in_progress(context, *current_user);
}
NPPS_ENDPOINT("live", "gameover", User, live_gameover);

static LiveRewardResponse live_reward(Context &context, const LiveRewardRequest &request) {
    // FIXME: Perform validation to keep cheaters away?
    // FIXME: Modularize this thing for MedFes, ChaFest, Score Match, etc.

    db::main::User *current_user_ptr = sys::user::get_current(context);
    db::main::User &current_user = *current_user_ptr;
    db::main::LiveInProgress *live_in_progress = sys::live::get_live_in_progress(context, current_user);
    if (live_in_progress == nullptr)
        throw error::IdolError(200, 720, "attempt to finish live show without playing", 403);

    const sys::live::CommonLive *live_difficulty_info =
        sys::live::get_live_info_table(context, request.live_difficulty_id);
    if (live_difficulty_info == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);

    const db::live::LiveSetting *live_setting_ptr =
        sys::live::get_live_setting_from_difficulty_id(context, request.live_difficulty_id);
    if (live_setting_ptr == nullptr) throw error::by_code(error::ERROR_CODE_LIVE_NOT_FOUND);
    const db::live::LiveSetting &live_setting = *live_setting_ptr;

    auto beatmap_data = sys::live::get_live_info(context, request.live_difficulty_id, live_setting);
    if (!beatmap_data) throw error::by_code(error::ERROR_CODE_LIVE_NOTES_LIST_NOT_FOUND);

    if (request.max_combo > (int64_t)beatmap_data->notes_list.size()) throw error::IdolError(200, 720, "...", 423);

    // The row is deleted below; NPPS4 expunges it from the session first.
    const int64_t lp_factor = live_in_progress->lp_factor;
    const int64_t unit_deck_id = live_in_progress->unit_deck_id;
    const Bytes deck_data_bytes = live_in_progress->deck_data;

    // Get old data
    db::main::LiveClear *live_clear_data =
        sys::live::get_live_clear_data(context, current_user, request.live_difficulty_id, true);
    const db::main::LiveClear old_live_clear_data = *live_clear_data;
    sys::user::UserInfoData old_user_info = sys::user::get_user_info(context, current_user);

    // Update live clear data
    int64_t score = request.score_smile + request.score_cute + request.score_cool;
    live_clear_data->hi_score = std::max(live_clear_data->hi_score, score);
    live_clear_data->hi_combo_cnt = std::max(live_clear_data->hi_combo_cnt, request.max_combo);
    live_clear_data->clear_cnt = live_clear_data->clear_cnt + 1;

    std::vector<model::Ptr<sys::item_model::Item>> special_reward;
    if (live_clear_data->clear_cnt == 1) {
        // First clear. Give loveca only if:
        // * User cleared Easy, Normal, and Hard
        // * User cleared Expert
        // * User cleared Master
        if (live_setting.difficulty > 3) {
            // User cleared Expert or higher. Give loveca directly.
            special_reward.push_back(sys::item::loveca(1));
        } else {
            // User cleared Easy, Normal, or Hard. Only give loveca if all is cleared.
            std::map<int64_t, int64_t> enh_list = sys::live::get_enh_live_difficulty_ids(context, request.live_difficulty_id);
            enh_list[live_setting.difficulty] = request.live_difficulty_id;
            bool cleared = true;

            for (int64_t i = 1; i < 4; i++) {
                db::main::LiveClear *live_clear_data_adjacent =
                    sys::live::get_live_clear_data(context, current_user, enh_list.at(i));
                if (live_clear_data_adjacent == nullptr || live_clear_data_adjacent->clear_cnt == 0) {
                    cleared = false;
                    break;
                }
            }

            if (cleared) special_reward.push_back(sys::item::loveca(1));
        }
    }

    // Get accomplished live goals
    std::vector<int64_t> old_goals_list = sys::live::get_achieved_goal_id_list(context, old_live_clear_data);
    std::vector<int64_t> new_goals_list = sys::live::get_achieved_goal_id_list(context, *live_clear_data);
    std::set<int64_t> old_live_goals(old_goals_list.begin(), old_goals_list.end());
    std::set<int64_t> new_live_goals(new_goals_list.begin(), new_goals_list.end());
    std::vector<int64_t> accomplished_live_goals;
    for (int64_t g : new_live_goals)
        if (!old_live_goals.count(g)) accomplished_live_goals.push_back(g);  // sorted
    auto live_goal_rewards = sys::live::get_goal_rewards(context, accomplished_live_goals);
    auto [score_rank, combo_rank, clear_rank] = sys::live::get_live_ranks(
        *live_difficulty_info, live_setting, score, request.max_combo, live_clear_data->clear_cnt);
    (void)clear_rank;

    // Give live goal rewards
    for (const auto &reward_data : live_goal_rewards) sys::advanced::add_item(context, current_user, reward_data);
    for (const auto &reward_data : special_reward) sys::advanced::add_item(context, current_user, reward_data);

    // This is the intended EXP and G drop
    int64_t target_difficulty_index = std::min(std::max(live_setting.difficulty, (int64_t)1), (int64_t)4) - 1;
    int64_t given_exp = const_::LIVE_EXP_DROP.at(target_difficulty_index);
    int64_t given_g = const_::LIVE_GOLD_DROP.at(target_difficulty_index).at(score_rank - 1);

    // Roll unit for live clear
    int64_t unit_id = ::npps::external::live_unit_drop::get_live_drop_unit(live_setting.live_setting_id, context);

    // FIXME: Drop different kinds of levels. Currently it's fixed at level 1.
    // FIXME: It does not show a new character screen. There must be a flag for it.
    sys::unit::QuickAddResult live_clear_drop = sys::unit::quick_create_by_unit_add(context, current_user, unit_id);
    std::optional<sys::unit::QuickAddResult> live_combo_drop, live_score_drop;
    if (request.max_combo >= live_setting.c_rank_combo) {
        unit_id = ::npps::external::live_unit_drop::get_live_drop_unit(live_setting.live_setting_id, context);
        live_combo_drop = sys::unit::quick_create_by_unit_add(context, current_user, unit_id);
    }
    if (score >= live_setting.c_rank_score) {
        unit_id = ::npps::external::live_unit_drop::get_live_drop_unit(live_setting.live_setting_id, context);
        live_score_drop = sys::unit::quick_create_by_unit_add(context, current_user, unit_id);
    } else {
        given_exp = (int64_t)std::ceil((double)given_exp / 2);
    }

    // Add user EXP
    given_exp = given_exp * lp_factor;
    std::vector<sys::user::NextLevelInfo> next_level_info = sys::user::add_exp(context, current_user, given_exp);
    current_user.game_coin = current_user.game_coin + given_g;

    // Add units
    int64_t unit_expiry = util::time() + const_::COMMON_UNIT_EXPIRY;
    int64_t current_unit_count = sys::unit::count_units(context, current_user, true);
    for (sys::unit::QuickAddResult *reward_data : {&live_clear_drop, live_combo_drop ? &*live_combo_drop : nullptr,
                                                   live_score_drop ? &*live_score_drop : nullptr}) {
        if (reward_data != nullptr)
            current_unit_count = sys::unit::process_quick_add(context, current_user, *reward_data, current_unit_count,
                                                              "FIXME live show reward JP text", "Live Show! Reward",
                                                              unit_expiry);
    }

    // Add bond
    int64_t love_count =
        (int64_t)std::ceil((double)(request.love_cnt * lp_factor) * config::get().love_multiplier);
    auto before_after_loves = sys::unit::add_love_by_deck(context, current_user, unit_deck_id, love_count);

    // Add live effort
    sys::effort::EffortResult effort_result = sys::effort::add_effort_items(context, current_user, score * lp_factor);
    for (auto &rewards : effort_result.rewards) {
        for (auto &r : rewards) {
            sys::advanced::AddResult add_result = sys::advanced::add_item(context, current_user, r);
            if (!add_result.success) {
                // TODO: Message
                sys::reward::add_item(context, current_user, r, "FIXME: Live Show! Clear message for JP",
                                      "Live Show! Clear");
                r->reward_box_flag = true;
            }
        }
    }
    effort_result.refresh_rewards();

    // Get current deck
    auto current_deck = sys::unit::load_unit_deck(context, current_user, unit_deck_id);
    std::vector<int64_t> subscenario_unlocks;
    if (!current_deck) throw std::logic_error("current_deck is not None");
    std::vector<sys::unit_model::UnitInfoData> unit_deck_full_info;
    std::vector<int64_t> unit_deck_unit_ids;
    for (int64_t unit_owning_user_id : std::get<1>(*current_deck)) {
        db::main::Unit *unit_data = sys::unit::get_unit(context, unit_owning_user_id);
        sys::unit::validate_unit(current_user, unit_data);

        const db::unit::Unit *unit_info = sys::unit::get_unit_info(context, unit_data->unit_id);
        if (unit_info == nullptr) throw std::invalid_argument("invalid unit_info (is db corrupt?)");

        // Try to unlock subscenario
        const db::unit::Rarity *unit_rarity = sys::unit::get_unit_rarity(context, unit_info->rarity);
        if (unit_rarity == nullptr) throw std::invalid_argument("invalid unit_rarity (is db corrupt?)");

        if (unit_data->love >= unit_rarity->after_love_max) {
            int64_t subscenario_id = sys::subscenario::get_subscenario_id_of_unit_id(context, unit_data->unit_id);
            if (subscenario_id > 0 && sys::subscenario::unlock(context, current_user, subscenario_id))
                subscenario_unlocks.push_back(subscenario_id);
        }

        unit_deck_unit_ids.push_back(unit_data->unit_id);
        unit_deck_full_info.push_back(std::get<0>(sys::unit::get_unit_data_full_info(context, *unit_data)));
    }

    // Check achievement
    auto [unit_ids_tuple, unit_type_ids_tuple] = sys::unit::tupleize_unit_id_and_type(context, unit_deck_unit_ids);
    sys::achievement::AchievementUpdateLiveClear live_ach_update;
    live_ach_update.live_track_id = live_setting.live_track_id;
    live_ach_update.difficulty = live_setting.difficulty;
    live_ach_update.attribute = live_setting.attribute_icon_id;
    live_ach_update.score_rank = score_rank;
    live_ach_update.combo_rank = combo_rank;
    live_ach_update.swing = live_setting.swing_flag != 0;
    live_ach_update.team_unit_ids = unit_ids_tuple;
    live_ach_update.team_unit_type_ids = unit_type_ids_tuple;
    std::vector<sys::achievement::AchievementUpdate> achievement_update = {
        live_ach_update,
        sys::achievement::AchievementUpdateNewUnit{},
        sys::achievement::AchievementUpdateUnitMaxLove{},
    };
    if (next_level_info.size() > 1) {
        sys::achievement::AchievementUpdateLevelUp level_up;
        level_up.rank = current_user.level;
        achievement_update.push_back(level_up);
    }

    sys::achievement::AchievementContext accomplished_achievement =
        sys::achievement::check(context, current_user, achievement_update);
    accomplished_achievement.fix();

    // Process achievement rewards part 1
    std::vector<std::vector<model::Ptr<sys::item_model::Item>>> accomplished_achievement_rewards;
    for (db::main::Achievement *ach : accomplished_achievement.accomplished)
        accomplished_achievement_rewards.push_back(sys::achievement::get_achievement_rewards(context, *ach));
    auto temp_achievement_rewards =
        sys::advanced::fixup_achievement_reward(context, current_user, accomplished_achievement_rewards);

    // Check achievement part 2
    std::vector<sys::achievement::AchievementUpdate> scenario_updates;
    for (const auto &reward_list : temp_achievement_rewards) {
        for (const auto &reward_data : reward_list) {
            if (reward_data->add_type == const_::ADD_TYPE::SCENARIO) {
                sys::achievement::AchievementUpdateItemCollect collect;
                collect.add_type = const_::ADD_TYPE::SCENARIO;
                collect.item_id = reward_data->item_id;
                scenario_updates.push_back(collect);
            }
        }
    }

    accomplished_achievement.extend(sys::achievement::check(context, current_user, scenario_updates));

    // Process achievement rewards part 2
    accomplished_achievement_rewards.clear();
    for (db::main::Achievement *ach : accomplished_achievement.accomplished)
        accomplished_achievement_rewards.push_back(sys::achievement::get_achievement_rewards(context, *ach));
    std::vector<std::vector<model::Ptr<sys::item_model::Item>>> new_achievement_rewards;
    for (db::main::Achievement *ach : accomplished_achievement.new_)
        new_achievement_rewards.push_back(sys::achievement::get_achievement_rewards(context, *ach));
    accomplished_achievement_rewards =
        sys::advanced::fixup_achievement_reward(context, current_user, accomplished_achievement_rewards);
    new_achievement_rewards = sys::advanced::fixup_achievement_reward(context, current_user, new_achievement_rewards);

    // Give achievement rewards
    sys::achievement::process_achievement_reward(context, current_user, accomplished_achievement.accomplished,
                                                 accomplished_achievement_rewards);

    // Clean live in progress
    sys::live::clean_live_in_progress(context, current_user);

    // Store precise log score
    if (truthy(request.precise_score_log.at("live_setting").at("precise_score_auto_update_flag"))) {
        Json precise_score_log = request.precise_score_log;
        precise_score_log["deck_info"] = Json::parse(deck_data_bytes);
        precise_score_log["max_combo"] = request.max_combo;
        sys::live::record_precise_score(context, current_user, request.live_difficulty_id,
                                        truthy(precise_score_log.at("is_skill_on")), beatmap_data->notes_list,
                                        precise_score_log);
    }

    // Log current player ranking
    sys::ranking::increment_daily_score(context, current_user, score);

    // Create response
    LiveRewardUnitList reward_unit_list;
    reward_unit_list.live_clear.push_back(live_clear_drop.as_item_reward->to_json());
    if (live_combo_drop) reward_unit_list.live_combo.push_back(live_combo_drop->as_item_reward->to_json());
    if (live_score_drop) reward_unit_list.live_rank.push_back(live_score_drop->as_item_reward->to_json());
    sys::user::UserInfoData user_info = sys::user::get_user_info(context, current_user);

    LiveRewardResponse response;
    response.live_info.push_back(model::make<LiveInfo>(
        sys::live::get_live_info_without_notes(context, request.live_difficulty_id, live_setting)));
    response.rank = score_rank;
    response.combo_rank = combo_rank;
    response.total_love = love_count;
    response.is_high_score = live_clear_data->hi_score > old_live_clear_data.hi_score;
    response.hi_score = live_clear_data->hi_score;
    response.base_reward_info.game_coin = given_g;
    response.base_reward_info.game_coin_reward_box_flag = false;
    response.base_reward_info.player_exp = given_exp;
    response.base_reward_info.player_exp_unit_max.before = old_user_info.unit_max;
    response.base_reward_info.player_exp_unit_max.after = user_info.unit_max;
    response.base_reward_info.player_exp_friend_max.before = old_user_info.friend_max;
    response.base_reward_info.player_exp_friend_max.after = user_info.friend_max;
    response.base_reward_info.player_exp_lp_max.before = old_user_info.energy_max;
    response.base_reward_info.player_exp_lp_max.after = user_info.energy_max;
    response.base_reward_info.social_point = 0;  // TODO: Add actual social point
    response.reward_unit_list = std::move(reward_unit_list);
    response.unlocked_subscenario_ids = std::move(subscenario_unlocks);
    response.effort_point = std::move(effort_result.infos);
    for (size_t i = 0; i < unit_deck_full_info.size(); i++)
        response.unit_list.push_back(
            unit_list_from_unit_data(unit_deck_full_info[i], (int64_t)i + 1, before_after_loves.before.at(i)));
    response.before_user_info = std::move(old_user_info);
    response.after_user_info = sys::user::get_user_info(context, current_user);
    response.next_level_info = std::move(next_level_info);
    response.goal_accomp_info.achieved_ids = accomplished_live_goals;
    response.goal_accomp_info.rewards = dump_items_list(live_goal_rewards);
    response.special_reward_info = dump_items_list(special_reward);
    response.accomplished_achievement_list = sys::achievement::to_game_representation(
        context, accomplished_achievement.accomplished, accomplished_achievement_rewards);
    response.unaccomplished_achievement_cnt = sys::achievement::get_achievement_count(context, current_user, false);
    response.added_achievement_list =
        sys::achievement::to_game_representation(context, accomplished_achievement.new_, new_achievement_rewards);
    response.new_achievement_cnt = (int64_t)accomplished_achievement.new_.size();
    response.museum_info = sys::museum::get_museum_info_data(context, current_user);
    response.present_cnt = sys::reward::count_presentbox(context, current_user);
    return response;
}
NPPS_ENDPOINT("live", "reward", User, live_reward);

}  // namespace npps::game::live
