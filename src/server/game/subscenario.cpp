// npps4/game/subscenario.py
#include "app/idol.hpp"
#include "system/const.hpp"
#include "system/item.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/subscenario.hpp"
#include "system/user.hpp"

namespace npps::game::subscenario {

static SubScenarioStatusResponse subscenario_status(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<db::main::SubScenario *> subscenarios = system::subscenario::get_all(context, *current_user);
    SubScenarioStatusResponse response;
    for (const db::main::SubScenario *sc : subscenarios)
        response.subscenario_status_list.push_back(
            SubScenarioStatus::make(sc->subscenario_id, 1 + (int64_t)sc->completed));
    return response;
}
NPPS_ENDPOINT("subscenario", "subscenarioStatus", User, subscenario_status);

static SubScenarioStartupResponse subscenario_startup(Context &context, const SubScenarioStartupRequest &request) {
    // Sanity check
    if (!system::subscenario::valid(context, request.subscenario_id))
        throw error::by_code(error::ERROR_CODE_SUBSCENARIO_NOT_FOUND);

    // Sanity check #2
    db::main::User *current_user = system::user::get_current(context);
    if (!system::subscenario::is_unlocked(context, *current_user, request.subscenario_id))
        throw error::by_code(error::ERROR_CODE_SUBSCENARIO_NOT_AVAILABLE);

    return SubScenarioStartupResponse::make(request.subscenario_id);
}
NPPS_ENDPOINT("subscenario", "startup", User, subscenario_startup);

static SubScenarioRewardResponse subscenario_reward(Context &context, const SubScenarioRewardRequest &request) {
    // Sanity check
    if (!system::subscenario::valid(context, request.subscenario_id))
        throw error::by_code(error::ERROR_CODE_SUBSCENARIO_NOT_FOUND);

    // Sanity check #2
    db::main::User *current_user = system::user::get_current(context);
    if (!system::subscenario::is_unlocked(context, *current_user, request.subscenario_id))
        throw error::by_code(error::ERROR_CODE_SUBSCENARIO_NOT_AVAILABLE);
    if (system::subscenario::is_completed(context, *current_user, request.subscenario_id))
        throw error::by_code(error::ERROR_CODE_SUBSCENARIO_NO_REWARD);

    // Capture old user info
    system::user::UserInfoData old_user_info = system::user::get_user_info(context, *current_user);

    // Add G and loveca
    current_user->free_sns_coin = current_user->free_sns_coin + const_::SUBSCENARIO_LOVECA_REWARD_AMOUNT;
    current_user->game_coin = current_user->game_coin + const_::SUBSCENARIO_GAME_COIN_REWARD_AMOUNT;
    model::Ptr<system::item_model::Item> loveca_reward = system::item::loveca(const_::SUBSCENARIO_LOVECA_REWARD_AMOUNT);
    loveca_reward->comment = context.is_lang_jp() ? const_::SUBSCENARIO_REWARD_COMMENT_JP
                                                  : const_::SUBSCENARIO_REWARD_COMMENT_EN;

    // Mark as complete
    system::subscenario::complete(context, *current_user, request.subscenario_id);

    SubScenarioRewardResponse response;
    response.clear_subscenario = model::make<SubScenarioRewardRequest>(request);
    response.before_user_info = old_user_info;
    response.after_user_info = system::user::get_user_info(context, *current_user);
    response.next_level_info = system::user::add_exp(context, *current_user, 0);
    response.base_reward_info = model::make<system::common::BaseRewardInfo>(
        system::common::BaseRewardInfo::make(const_::SUBSCENARIO_GAME_COIN_REWARD_AMOUNT, false));
    response.item_reward_info = {loveca_reward->to_json()};
    response.museum_info = system::museum::get_museum_info_data(context, *current_user);
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("subscenario", "reward", User, subscenario_reward);

}  // namespace npps::game::subscenario
