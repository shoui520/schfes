// npps4/game/common.py
#include "app/idol.hpp"
#include "system/item.hpp"
#include "system/live.hpp"
#include "system/reward.hpp"
#include "system/user.hpp"

namespace npps::game::common {

static void common_liveresume(Context &context, const CommonLiveResumeRequest &request) {
    if (request.cancel) {
        db::main::User *current_user = system::user::get_current(context);
        system::live::clean_live_in_progress(context, *current_user);
    }
}
NPPS_ENDPOINT("common", "liveResume", User, common_liveresume);

static CommonRecoveryEnergyResponse common_recoveryenergy(Context &context,
                                                          const CommonRecoveryEnergyRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    int64_t current_loveca = system::user::get_loveca(*current_user);

    if (request.energy_type != 1) {
        // TODO
        throw error::by_code(error::ERROR_CODE_ENERGY_FULL);
    }
    if (request.amount < 0) throw error::IdolError(200, 720, "Amount out of range");

    if (request.item_id == 0) {
        if (current_loveca >= request.amount) {
            // Loveca, behave as restore 100% LP
            system::user::add_energy_percentage(*current_user, 1.0, request.amount);
            system::user::sub_loveca(*current_user, request.amount);
        } else {
            throw error::by_code(error::ERROR_CODE_NOT_ENOUGH_LOVECA);
        }
    } else {
        // LP recover item
        const db::item::RecoveryItem *recovery_item_info = system::item::get_recovery_item_info(context, request.item_id);
        if (recovery_item_info == nullptr) throw error::by_code(error::ERROR_CODE_NO_RECOVER_ITEM);

        db::main::RecoveryItem *recovery_item_data =
            system::item::get_recovery_item_data_guaranteed(context, *current_user, request.item_id);
        if (recovery_item_data->amount >= request.amount) {
            switch (recovery_item_info->recovery_type) {
            case 1:
                // Percentage
                system::user::add_energy_percentage(*current_user, (double)recovery_item_info->recovery_value / 100,
                                                    request.amount);
                break;
            case 2:
                // Exact
                system::user::add_energy(*current_user, recovery_item_info->recovery_value * request.amount);
                break;
            default: throw error::IdolError(200, 720, "Unknown recovery type");
            }
            recovery_item_data->amount = recovery_item_data->amount - request.amount;
        } else {
            throw error::by_code(error::ERROR_CODE_RECOVER_ITEM_NOT_ENOUGH);
        }
    }

    system::user::UserInfoData user_info = system::user::get_user_info(context, *current_user);
    CommonRecoveryEnergyResponse response;
    response.item_list = system::item::get_recovery_items(context, *current_user);
    response.energy_max = user_info.energy_max;
    response.over_max_energy = user_info.over_max_energy;
    response.energy_full_time = user_info.energy_full_time;
    response.before_sns_coin = current_loveca;
    response.after_sns_coin = user_info.sns_coin;
    response.before_game_coin = user_info.game_coin;
    response.after_game_coin = user_info.game_coin;  // Game coin is unused in this scenario
    response.training_energy = user_info.training_energy;
    response.training_energy_max = user_info.training_energy_max;
    response.present_cnt = system::reward::count_presentbox(context, *current_user);
    return response;
}
NPPS_ENDPOINT("common", "recoveryEnergy", User, common_recoveryenergy, .batchable = false);

}  // namespace npps::game::common
