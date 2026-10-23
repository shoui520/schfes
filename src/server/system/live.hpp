#pragma once
// npps4/system/live.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <array>
#include <map>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace npps::system::live {

using live_model::LiveInfo;
using live_model::LiveInfoWithNotes;
using live_model::LiveNote;
using live_model::LiveStatus;

// db.live.CommonLive: the columns NormalLive and SpecialLive share. Exactly
// one of `normal`/`special` is set.
struct CommonLive {
    const db::live::NormalLive *normal = nullptr;
    const db::live::SpecialLive *special = nullptr;
    int64_t live_difficulty_id = 0;
    int64_t live_setting_id = 0;
    int64_t capital_type = 0;
    int64_t capital_value = 0;
    int64_t c_rank_complete = 0;
    int64_t b_rank_complete = 0;
    int64_t a_rank_complete = 0;
    int64_t s_rank_complete = 0;
};

// Python range(start, stop).
struct Range {
    int64_t start = 0, stop = 0;
    bool contains(int64_t value) const { return value >= start && value < stop; }
};

bool unlock_normal_live(Context &context, db::main::User &user, int64_t live_track_id);
void init(Context &context, db::main::User &user);

// Null only when ensure is false. Throws std::invalid_argument for an invalid live_difficulty_id when ensuring.
db::main::LiveClear *get_live_clear_data(Context &context, db::main::User &user, int64_t live_difficulty_id,
                                         bool ensure = false);
std::vector<db::main::LiveClear *> get_all_live_clear_data(Context &context, db::main::User &user);
LiveStatus live_status_from_live_clear(Context &context, int64_t live_difficulty_id, db::main::LiveClear *live_clear,
                                       int64_t missing_status = 1);
std::vector<LiveStatus> get_normal_live_clear_status(Context &context, db::main::User &user);
std::vector<LiveStatus> get_normal_live_clear_status_of_track(Context &context, db::main::User &user,
                                                              int64_t live_track_id);

// SpecialLive first, then NormalLive; null when neither exists.
const CommonLive *get_live_info_table(Context &context, int64_t live_difficulty_id);
const db::live::LiveSetting *get_live_setting(Context &context, int64_t live_setting_id);
std::optional<int64_t> get_live_lp(Context &context, int64_t live_difficulty_id);
const db::live::LiveSetting *get_live_setting_from_difficulty_id(Context &context, int64_t live_difficulty_id);
// nullopt when the beatmap is not found.
std::optional<LiveInfoWithNotes> get_live_info(Context &context, int64_t live_difficulty_id,
                                               const db::live::LiveSetting &live_setting);
LiveInfo get_live_info_without_notes(Context &context, int64_t live_difficulty_id,
                                     const db::live::LiveSetting &live_setting);
std::vector<const db::live::LiveGoalReward *> get_goal_list_by_live_difficulty_id(Context &context,
                                                                                  int64_t live_difficulty_id);

constexpr int64_t MAX_INT = 2147483647;

std::map<const_::LIVE_GOAL_TYPE, std::vector<Range>> make_rank_range(const CommonLive &live_info,
                                                                     const db::live::LiveSetting &live_setting);
int64_t get_index_of_range(int64_t value, const std::vector<Range> &seq, int64_t start = 0, int64_t default_ = -1);

constexpr std::array<const_::LIVE_GOAL_TYPE, 3> LIVE_GOAL_TYPES = {
    const_::LIVE_GOAL_TYPE::SCORE, const_::LIVE_GOAL_TYPE::COMBO, const_::LIVE_GOAL_TYPE::CLEAR};

// (score_rank, combo_rank, clear_rank)
std::tuple<int64_t, int64_t, int64_t> get_live_ranks(const CommonLive &live_info,
                                                     const db::live::LiveSetting &live_setting, int64_t score,
                                                     int64_t combo, int64_t clears);
std::vector<int64_t> get_achieved_goal_id_list(Context &context, const db::main::LiveClear &clear_info);
// Plain item_model::Item objects.
std::vector<model::Ptr<item_model::Item>> get_goal_rewards(Context &context, const std::vector<int64_t> &goal_ids);
std::vector<int64_t> get_live_setting_ids_from_track_id(Context &context, int64_t live_track_id);
bool has_normal_live_unlock(Context &context, db::main::User &user, int64_t live_track_id);
std::vector<int64_t> get_all_normal_live_unlock(Context &context, db::main::User &user);
db::main::LiveInProgress *get_live_in_progress(Context &context, db::main::User &user);  // nullable
bool clean_live_in_progress(Context &context, db::main::User &user);
void register_live_in_progress(Context &context, db::main::User &user, db::main::User &party_user, int64_t lp_factor,
                               int64_t unit_deck_id, const Bytes &deck_data_bytes);
// Throws std::invalid_argument when not found.
const db::live::LiveTrack *get_live_track_info(Context &context, int64_t live_track_id);

// dict[rotation_group_id, list[(live_difficulty_id, base_day)]] in insertion order.
std::vector<std::pair<int64_t, std::vector<std::pair<int64_t, int64_t>>>>
get_special_live_rotation_time_modulo(Context &context);
// dict[rotation_group_id, live_difficulty_id] in insertion order.
std::vector<std::pair<int64_t, int64_t>> get_special_live_rotation_difficulty_id(Context &context);
std::vector<LiveStatus> get_special_live_status(Context &context, db::main::User &user);
std::vector<int64_t> get_training_live_difficulty_id_from_live_track_id(Context &context, int64_t live_track_id);
std::vector<LiveStatus> get_training_live_clear_status_of_track(Context &context, db::main::User &user,
                                                                int64_t live_track_id);
std::vector<LiveStatus> get_training_live_status(Context &context, db::main::User &user);

// NotesListRoot: model_dump() / model_dump_json() / model_validate_json() of list[LiveNote].
Json notes_list_dump(const std::vector<LiveNote> &notes_list);
// model_dump_json() formatting of a dumped model: compact separators, floats as pydantic prints them.
std::string compact_json_dump(const Json &value);
std::string notes_list_dump_json(const std::vector<LiveNote> &notes_list);  // compact, pydantic separators
std::vector<LiveNote> notes_list_validate_json(std::string_view json);

// (crc32, sha256)
std::tuple<int64_t, Bytes> record_notes_list(Context &context, const std::vector<LiveNote> &notes_list);
void record_precise_score(Context &context, db::main::User &user, int64_t live_difficulty_id, bool use_skill,
                          const std::vector<LiveNote> &notes_list, const Json &precise_log_data);
// (precise_log, notes_list, timestamp) or nullopt.
std::optional<std::tuple<Json, std::vector<LiveNote>, int64_t>>
pull_precise_score_with_beatmap(Context &context, db::main::User &user, int64_t live_difficulty_id, bool use_skill);
// difficulty -> count
std::map<int64_t, int64_t> get_cleared_live_count(Context &context, db::main::User &user);
// difficulty (<= 3) -> live_difficulty_id. Throws std::invalid_argument for an invalid live_difficulty_id.
std::map<int64_t, int64_t> get_enh_live_difficulty_ids(Context &context, int64_t live_difficulty_id);

}  // namespace npps::system::live
