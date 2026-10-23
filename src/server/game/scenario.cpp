// npps4/game/scenario.py
#include "app/idol.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/scenario.hpp"
#include "system/user.hpp"

namespace npps::game::scenario {

static ScenarioStatusResponse scenario_scenariostatus(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<db::main::Scenario *> scenarios = system::scenario::get_all(context, *current_user);
    ScenarioStatusResponse response;
    for (const db::main::Scenario *sc : scenarios)
        response.scenario_status_list.push_back(ScenarioStatus::make(sc->scenario_id, 1 + (int64_t)sc->completed));
    return response;
}
NPPS_ENDPOINT("scenario", "scenarioStatus", User, scenario_scenariostatus);

static ScenarioStartupResponse scenario_startup(Context &context, const ScenarioStartupRequest &request) {
    // Sanity check
    if (!system::scenario::valid(context, request.scenario_id)) throw error::by_code(error::ERROR_CODE_SCENARIO_NOT_FOUND);

    // Sanity check #2
    db::main::User *current_user = system::user::get_current(context);
    if (!system::scenario::is_unlocked(context, *current_user, request.scenario_id))
        throw error::by_code(error::ERROR_CODE_SCENARIO_NOT_AVAILABLE);

    return ScenarioStartupResponse::make(request.scenario_id);
}
NPPS_ENDPOINT("scenario", "startup", User, scenario_startup);

static ScenarioRewardResponse scenario_reward(Context &context, const ScenarioRewardRequest &request) {
    namespace achievement = system::achievement;

    // Sanity check
    if (!system::scenario::valid(context, request.scenario_id)) throw error::by_code(error::ERROR_CODE_SCENARIO_NOT_FOUND);

    // Sanity check #2
    db::main::User *current_user = system::user::get_current(context);
    if (!system::scenario::is_unlocked(context, *current_user, request.scenario_id))
        throw error::by_code(error::ERROR_CODE_SCENARIO_NOT_AVAILABLE);
    if (system::scenario::is_completed(context, *current_user, request.scenario_id))
        throw error::by_code(error::ERROR_CODE_SCENARIO_NO_REWARD);

    // Capture old user info
    system::user::UserInfoData old_user_info = system::user::get_user_info(context, *current_user);

    // Mark as complete
    system::scenario::complete(context, *current_user, request.scenario_id);

    // Trigger achievement
    achievement::AchievementContext achievement_list = achievement::check(
        context, *current_user,
        {achievement::AchievementUpdateFinishScenario{request.scenario_id},
         achievement::AchievementUpdateLevelUp{current_user->level}});

    // Give achievement rewards
    std::vector<std::vector<model::Ptr<system::item_model::Item>>> accomplished_rewards, unaccomplished_rewards;
    for (const db::main::Achievement *ach : achievement_list.accomplished)
        accomplished_rewards.push_back(achievement::get_achievement_rewards(context, *ach));
    for (const db::main::Achievement *ach : achievement_list.new_)
        unaccomplished_rewards.push_back(achievement::get_achievement_rewards(context, *ach));
    accomplished_rewards = system::advanced::fixup_achievement_reward(context, *current_user, accomplished_rewards);
    unaccomplished_rewards = system::advanced::fixup_achievement_reward(context, *current_user, unaccomplished_rewards);
    achievement::process_achievement_reward(context, *current_user, achievement_list.accomplished,
                                            accomplished_rewards);

    ScenarioRewardResponse response;
    response.clear_scenario = model::make<ScenarioRewardRequest>(request);
    response.before_user_info = old_user_info;
    response.after_user_info = system::user::get_user_info(context, *current_user);
    response.next_level_info = system::user::add_exp(context, *current_user, 0);
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    response.accomplished_achievement_list =
        achievement::to_game_representation(context, achievement_list.accomplished, accomplished_rewards);
    response.unaccomplished_achievement_cnt = achievement::get_achievement_count(context, *current_user, false);
    response.added_achievement_list =
        achievement::to_game_representation(context, achievement_list.new_, unaccomplished_rewards);
    response.new_achievement_cnt = (int64_t)achievement_list.new_.size();
    return response;
}
NPPS_ENDPOINT("scenario", "reward", User, scenario_reward);

}  // namespace npps::game::scenario
