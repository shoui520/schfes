#pragma once
// npps4/system/unit.py
#include "app/context.hpp"
#include "app/models.hpp"
#include "system/unit_model.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace npps::system::unit {

struct UnitStatsResult {
    int64_t level = 0;
    int64_t smile = 0;
    int64_t pure = 0;
    int64_t cool = 0;
    int64_t hp = 0;
    int64_t next_exp = 0;
    int64_t merge_exp = 0;
    int64_t merge_cost = 0;
    int64_t sale_price = 0;
};

struct UnitStatsCalculationID {
    int64_t unit_id = 0;
    int64_t exp = 0;
    int64_t max_level = 0;
    int64_t level_limit_id = 0;
    auto operator<=>(const UnitStatsCalculationID &) const = default;
    static UnitStatsCalculationID from_unit_data(const db::main::Unit &unit_data);
};

// Master data pattern lists (valid until the end of the request).
using LevelUpPatterns = std::vector<const db::unit::UnitLevelUpPattern *>;
using LevelLimitPatterns = std::vector<const db::unit::LevelLimitPattern *>;
using SkillLevelUpPatterns = std::vector<const db::unit::UnitSkillLevelUpPattern *>;

int64_t count_units(Context &context, const db::main::User &user, bool active);
// active == nullopt: every unit of the user.
std::vector<db::main::Unit *> get_all_units(Context &context, const db::main::User &user,
                                            std::optional<bool> active = std::nullopt);

// A transient (not yet added) unit row; nullopt when the unit cannot be ranked up (support member).
// user == nullptr: user_id 0. Throws std::invalid_argument for an invalid unit_id.
std::optional<db::main::Unit> create_unit(Context &context, const db::main::User *user, int64_t unit_id, bool active,
                                          int64_t level = 1, bool is_signed = false);
// session.add(unit_data) + album update + flush. Returns the persisted row (id set); the argument is copied
// because Session::add takes ownership of a copy.
db::main::Unit *add_unit_by_object(Context &context, db::main::User &user, const db::main::Unit &unit_data);
// Null when the unit is a support member.
db::main::Unit *add_unit_simple(Context &context, db::main::User &user, int64_t unit_id, bool active,
                                const unit_model::UnitExtraData &extra_data = unit_model::UnitExtraData_EMPTY);

// Throws error::by_code(ERROR_CODE_UNIT_NOT_EXIST) when missing.
db::main::Unit *get_unit(Context &context, int64_t unit_owning_user_id);
// unit_data may be null (Optional in Python).
void validate_unit(const db::main::User &user, const db::main::Unit *unit_data);

// Null when unit_id is not a support member (or missing with ensure == false).
db::main::UnitSupporter *get_supporter_unit(Context &context, const db::main::User &user, int64_t unit_id,
                                            bool ensure = false);
bool add_supporter_unit(Context &context, db::main::User &user, int64_t unit_id, int64_t quantity = 1);
bool sub_supporter_unit(Context &context, db::main::User &user, int64_t unit_id, int64_t quantity = 1);
// (unit_id, amount)
std::vector<std::tuple<int64_t, int64_t>> get_all_supporter_unit(Context &context, const db::main::User &user);
std::vector<unit_model::SupporterInfoResponse> get_unit_support_list_response(Context &context,
                                                                              const db::main::User &user);

// @context_cacheable("unit"). Never null; throws std::invalid_argument when the row does not exist.
const db::unit::Unit *get_unit_info(Context &context, int64_t unit_id);
// @context_cacheable("unit_rarity"). Null if missing.
const db::unit::Rarity *get_unit_rarity(Context &context, int64_t rarity);
// Null if missing.
const db::unit::Unit *get_unit_info_from_unit_number(Context &context, int64_t unit_number);
const LevelUpPatterns &get_unit_level_up_pattern(Context &context, int64_t unit_level_up_pattern_id);
const LevelLimitPatterns &get_unit_level_limit_pattern(Context &context, int64_t level_limit_id);
// Null for nullopt/0 or encrypted rows.
const db::unit::UnitSkill *get_unit_skill(Context &context, std::optional<int64_t> default_unit_skill_id);
const SkillLevelUpPatterns &get_unit_skill_level_up_pattern(Context &context, int64_t unit_skill);

// Returns true if the unit was in the deck (and clears it).
bool detach_from_deck_2(int64_t unit_owning_user_id, db::main::UnitDeck &deck);
void detach_from_all_deck(Context &context, db::main::User &user, const db::main::Unit &unit_data);
void remove_unit(Context &context, db::main::User &user, db::main::Unit *unit_data);

inline constexpr int64_t VALID_DECK_ID_FIRST = 1;
inline constexpr int64_t VALID_DECK_ID_LAST = 18;  // inclusive; range(1, 19)
// deck, unit_owning_user_ids (9 entries); nullopt when the deck does not exist and ensure is false.
// Throws std::invalid_argument when index is out of range.
std::optional<std::tuple<db::main::UnitDeck *, std::vector<int64_t>>> load_unit_deck(Context &context,
                                                                                      const db::main::User &user,
                                                                                      int64_t index,
                                                                                      bool ensure = false);
void save_unit_deck(Context &context, const db::main::User &user, db::main::UnitDeck &deck,
                    const std::vector<int64_t> &unit_owning_user_ids);
// (deck_number, name, unit_owning_user_id_1..9)
std::vector<std::tuple<int64_t, std::string, std::array<int64_t, 9>>> get_all_deck_simple(Context &context,
                                                                                         const db::main::User &user);
std::set<int64_t> find_all_valid_deck_number_ids(Context &context, const db::main::User &user);

void set_unit_center(Context &context, db::main::User &user, const db::main::Unit *unit_data);
int64_t get_unit_center(Context &context, const db::main::User &user, bool center_team_instead_of_partner = false);
bool idolize(Context &context, db::main::User &user, db::main::Unit &unit_data);

common::BeforeAfter<std::vector<int64_t>> add_love_by_deck(Context &context, db::main::User &user, int64_t deck_index,
                                                           int64_t love);

UnitStatsResult calculate_unit_stats(const db::unit::Unit &unit_info, const LevelUpPatterns &pattern, int64_t exp);
UnitStatsResult calculate_unit_stats(const db::unit::Unit &unit_info, const LevelLimitPatterns &pattern, int64_t exp);
int64_t get_exp_for_target_level(const db::unit::Unit &unit_info, const LevelUpPatterns &patterns, int64_t level);
int64_t get_exp_for_target_level(const db::unit::Unit &unit_info, const LevelLimitPatterns &patterns, int64_t level);
// unit_skill / pattern may be null. Returns (skill_level, next_exp).
std::tuple<int64_t, int64_t> calculate_unit_skill_stats(const db::unit::UnitSkill *unit_skill,
                                                        const SkillLevelUpPatterns *pattern, int64_t exp);

// @context_cacheable("unit_stats_calculated")
UnitStatsResult get_unit_stats_from_unit_data(Context &context, const UnitStatsCalculationID &calckey);
std::tuple<unit_model::UnitInfoData, UnitStatsResult> get_unit_data_full_info(Context &context,
                                                                              const db::main::Unit &unit_data);

// (smile, pure, cool)
std::tuple<int64_t, int64_t, int64_t> calculate_bonus_stat_of_removable_skill(
    const db::unit::RemovableSkill &removable_skill, const std::tuple<int64_t, int64_t, int64_t> &stats);

bool unit_type_has_tag(Context &context, int64_t unit_type_id, int64_t member_tag_id);
// Null if missing or encrypted.
const db::unit::LeaderSkill *get_leader_skill(Context &context, int64_t leader_skill);
const db::unit::ExtraLeaderSkill *get_extra_leader_skill(Context &context, int64_t leader_skill);

db::main::RemovableSkillInfo *get_removable_skill_info(Context &context, const db::main::User &user,
                                                       int64_t removable_skill_id);
// Null if missing or encrypted.
const db::unit::RemovableSkill *get_removable_skill_game_info(Context &context, int64_t removable_skill_id);
int64_t get_unit_removable_skill_count(Context &context, const db::main::User &user, int64_t removable_skill_id);
std::vector<db::main::RemovableSkillInfo *> get_all_unit_removable_skill(Context &context, const db::main::User &user);
std::vector<int64_t> get_unit_removable_skills(Context &context, const db::main::Unit &unit_data);
// {unit_owning_user_id: [unit_removable_skill_id]} in first-seen order.
std::vector<std::pair<int64_t, std::vector<int64_t>>> get_all_unit_removable_skills(Context &context,
                                                                                    const db::main::User &user);
int64_t add_unit_removable_skill(Context &context, db::main::User &user, int64_t removable_skill_id,
                                 int64_t amount = 1);
int64_t sub_unit_removable_skill(Context &context, db::main::User &user, int64_t removable_skill_id,
                                 int64_t amount = 1);
bool attach_unit_removable_skill(Context &context, const db::main::Unit &unit, int64_t removable_skill_id);
bool detach_unit_removable_skill(Context &context, const db::main::Unit &unit, int64_t removable_skill_id);
unit_model::RemovableSkillInfoResponse get_removable_skill_info_request(Context &context, const db::main::User &user);

// unit_to_item with the default cls=UnitItem.
model::Ptr<unit_model::UnitItem> unit_to_item(Context &context, const db::main::Unit &unit_data);

// @context_cacheable("unit_support_member"). Throws std::invalid_argument for an invalid unit_id.
bool is_support_member(Context &context, int64_t unit_id);

// Python dataclass QuickAddResult. unit_data is the transient row from create_unit (not yet added); after it
// is added with add_unit_by_object, pass the persisted row to update_unit_owning_user_id.
struct QuickAddResult {
    int64_t unit_id = 0;
    model::Ptr<unit_model::UnitSupportItem> as_item_reward;  // UnitSupportItem or UnitItem
    std::optional<db::main::Unit> unit_data;
    std::optional<unit_model::UnitInfoData> full_info;
    std::optional<UnitStatsResult> stats;

    // `added` is the persisted row of unit_data (Python reads unit_data.id).
    void update_unit_owning_user_id(const db::main::Unit &added);
};

QuickAddResult quick_create_by_unit_add(Context &context, db::main::User &user, int64_t unit_id, int64_t level = 1);
// current_unit_count == nullopt: just counts and returns the active unit count (as Python does).
int64_t process_quick_add(Context &context, db::main::User &user, QuickAddResult &quick_add_result,
                          std::optional<int64_t> current_unit_count = std::nullopt,
                          const std::string &reason_jp = "Reward", const std::string &reason_en = "Reward",
                          int64_t expire = 0);

bool has_signed_variant(Context &context, int64_t unit_id);
bool is_unit_max(Context &context, const db::main::User &user);
int64_t get_exchange_point_id_by_unit_id(Context &context, int64_t unit_id);
double get_exp_multiplier(const_::EVOLUTION_BONUS_TYPE evolution_type);

// unit_info/unit_rarity may be null (looked up from unit_data).
int64_t get_max_exp(Context &context, const db::main::Unit &unit_data, const db::unit::Unit *unit_info = nullptr,
                    const db::unit::Rarity *unit_rarity = nullptr);
// Null if missing or encrypted.
const db::unit::UnitSkillLevel *get_unit_skill_level_data(Context &context, int64_t unit_skill_id, int64_t level);
int64_t get_max_skill_exp(Context &context, const db::unit::Unit &unit_info);

// Returns a UnitItem, or a UnitSupportItem for support members.
model::Ptr<unit_model::UnitSupportItem> create_unit_item(
    Context &context, int64_t unit_id, int64_t amount = 1,
    const unit_model::UnitExtraData &extra_data = unit_model::UnitExtraData_EMPTY);
db::main::Unit create_unit_data(Context &context, const db::main::User &user, const unit_model::UnitItem &unit_item,
                                bool active = true);
model::Ptr<unit_model::UnitItem> unit_info_data_to_unit_item(Context &context,
                                                             const unit_model::UnitInfoData &unit_info_data);

// Copies the UnitItem/UnitSupportItem specific fields (everything except the Item fields) into other.extra.
void populate_unit_item_to_other(const model::Ptr<unit_model::UnitSupportItem> &unit_item,
                                 game::reward::IncentiveItem &other);

// (unit_ids[0..8], unit_type_ids[0..8]); unit_ids must hold at least 9 entries.
std::tuple<std::array<int64_t, 9>, std::array<int64_t, 9>> tupleize_unit_id_and_type(
    Context &context, const std::vector<int64_t> &unit_ids);

}  // namespace npps::system::unit
