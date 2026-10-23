#pragma once
// npps4/system/achievement.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <array>
#include <optional>
#include <set>
#include <tuple>
#include <variant>
#include <vector>

namespace npps::system::achievement {

extern const std::vector<item_model::BaseItem> ACHIEVEMENT_REWARD_DEFAULT;
inline constexpr std::array<const_::ADD_TYPE, 2> BACKGROUND_TITLE_ADD_TYPE = {const_::ADD_TYPE::BACKGROUND,
                                                                              const_::ADD_TYPE::AWARD};

// AchievementData.from_sqlalchemy
AchievementData from_sqlalchemy(const db::main::Achievement &ach, const db::achievement::Achievement &info,
                                const std::vector<model::Ptr<item_model::Item>> &rewards);

// Rows are owned by context.db.main; membership tests compare identity like SQLAlchemy objects.
struct AchievementContext {
    std::vector<db::main::Achievement *> accomplished;
    std::vector<db::main::Achievement *> new_;

    void extend(const AchievementContext &other);
    // Anything in "new" should be removed if it's in "accomplished"
    AchievementContext &fix();
    void add(db::main::Achievement *ach);
    AchievementContext operator+(const AchievementContext &other) const;
    bool has_achievement() const;
};

// Instantiate these to perform checks on achievements with the corresponding trigger.
struct AchievementUpdateLiveClear {  // "Live Clear"
    int64_t live_track_id = 0;
    int64_t difficulty = 0;
    int64_t attribute = 0;
    int64_t score_rank = 0;
    int64_t combo_rank = 0;
    bool swing = false;
    std::array<int64_t, 9> team_unit_ids{};
    std::array<int64_t, 9> team_unit_type_ids{};
};
struct AchievementUpdateSecretbox {  // "Scouting"
    int64_t secretbox_id = 0;
    int64_t amount = 0;
};
struct AchievementUpdateNewUnit {};  // "New Unique Unit"
struct AchievementUpdateUnitRankUp {  // "Idolize"
    std::vector<int64_t> unit_ids;
};
struct AchievementUpdateUnitMaxLevel {};  // "Max Level"
struct AchievementUpdateUnitMaxLove {};   // "Max Bond"
struct AchievementUpdateLevelUp {          // "Level Up"
    int64_t rank = 0;  // Current player rank
};
struct AchievementUpdateLoginBonus {  // "Login Bonus"
    int64_t login_days = 0;  // Total days logged in
};
struct AchievementUpdateUnitMerge {  // "Practice"
    int64_t unit_owning_user_id = 0;
    int64_t skill_level = 0;
    int64_t amount = 0;
};
struct AchievementUpdateFinishScenario {  // "Clearing Main Story"
    int64_t scenario_id = 0;  // Main story ID
};
struct AchievementUpdateItemCollect {  // "Got Item"
    const_::ADD_TYPE add_type{};
    int64_t item_id = 0;
    int64_t amount = 1;
};
struct AchievementUpdateAnywhere {};
struct AchievementUpdateAchievementComplete {
    std::vector<int64_t> completed_achievement_id;  // List of completed achievement IDs
};

// One element of check()'s *updates.
using AchievementUpdate =
    std::variant<AchievementUpdateLiveClear, AchievementUpdateSecretbox, AchievementUpdateNewUnit,
                 AchievementUpdateUnitRankUp, AchievementUpdateUnitMaxLevel, AchievementUpdateUnitMaxLove,
                 AchievementUpdateLevelUp, AchievementUpdateLoginBonus, AchievementUpdateUnitMerge,
                 AchievementUpdateFinishScenario, AchievementUpdateItemCollect, AchievementUpdateAnywhere,
                 AchievementUpdateAchievementComplete>;

// Never null; throws std::invalid_argument("invalid achievement").
const db::achievement::Achievement *get_achievement_info(Context &context, int64_t achievement_id);
std::vector<int64_t> get_next_achievement_ids(Context &context, int64_t achievement_id);
std::vector<int64_t> get_prerequisite_achievement_ids(Context &context, int64_t achievement_id);
// (unit_type_id, rank)
std::vector<std::tuple<int64_t, int64_t>> get_unit_type_groups(Context &context,
                                                                int64_t achievement_unit_type_group_id);
std::vector<int64_t> get_achievement_ids_from_category(Context &context, int64_t achievement_category_id);

db::main::Achievement *add_achievement(Context &context, db::main::User &user,
                                       const db::achievement::Achievement &ach,
                                       std::optional<int64_t> time = std::nullopt, bool flush = true);
bool has_achievement(Context &context, db::main::User &user, int64_t achievement_id);
void update_resettable_achievement(Context &context, db::main::User &user, std::optional<int64_t> ts = std::nullopt);
std::vector<AchievementData> to_game_representation(Context &context, const std::vector<db::main::Achievement *> &achs,
                                                    const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss);
std::vector<model::Ptr<item_model::Item>> get_achievement_rewards(Context &context, const db::main::Achievement &ach);
std::vector<model::Ptr<item_model::Item>> get_achievement_rewards(Context &context, int64_t ach_id);
void init(Context &context, db::main::User &user);
// Throws IdolError "invalid achievement id N" if missing or not owned by user.
db::main::Achievement *get_achievement(Context &context, db::main::User &user, int64_t ach_id);
std::vector<db::main::Achievement *> get_achievements(Context &context, db::main::User &user,
                                                      std::optional<bool> accomplished = std::nullopt);
std::vector<db::main::Achievement *> get_unclaimed_achievements(Context &context, db::main::User &user,
                                                                bool accomplished_but_unclaimed_only = false);
std::vector<db::main::Achievement *> get_unclaimed_achievements_by_filter_id(Context &context, db::main::User &user,
                                                                             int64_t achievement_filter_category_id);
std::vector<db::main::Achievement *> get_accomplished_achievements_by_filter_id(Context &context, db::main::User &user,
                                                                                int64_t achievement_filter_category_id);
void mark_achievement_reward_claimed(Context &context, db::main::Achievement &ach);
int64_t get_achievement_count(Context &context, db::main::User &user, std::optional<bool> accomplished = std::nullopt);
bool is_unlock_satisfied(Context &context, int64_t target_ach_id, const std::vector<int64_t> &unlocked_ach_ids);
std::vector<int64_t> get_unlocked_achievements_by_id(Context &context, db::main::User &target_user,
                                                     const std::vector<int64_t> &achievement_ids);
// check(context, user, *updates)
AchievementContext check(Context &context, db::main::User &target_user,
                         const std::vector<AchievementUpdate> &updates = {});
std::vector<int64_t> get_achievement_filter_ids(Context &context);
// (achievement_category_id, count)
std::vector<std::tuple<int64_t, int64_t>> count_accomplished_achievement_by_category(Context &context,
                                                                                    db::main::User &user);
int64_t count_accomplished_achievement_by_set(Context &context, db::main::User &user, const std::set<int64_t> &ach_ids);
void give_achievement_reward(Context &context, db::main::User &user, const db::achievement::Achievement &ach_info,
                             const std::vector<model::Ptr<item_model::Item>> &rewards);
void process_achievement_reward(Context &context, db::main::User &user,
                                const std::vector<db::main::Achievement *> &achievements,
                                const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss);

}  // namespace npps::system::achievement
