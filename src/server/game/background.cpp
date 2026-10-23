// npps4/game/background.py
#include "app/idol.hpp"
#include "system/background.hpp"
#include "system/user.hpp"

namespace npps::game::background {

static BackgroundInfoResponse background_backgroundinfo(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<db::main::Background *> backgrounds = system::background::get_backgrounds(context, *current_user);
    std::vector<BackgroundInfo> background_info;
    for (const db::main::Background *bg : backgrounds)
        background_info.push_back(BackgroundInfo::make(bg->background_id,
                                                       current_user->active_background == bg->background_id,
                                                       util::timestamp_to_datetime(bg->insert_date)));

    return BackgroundInfoResponse::make(background_info);
}
NPPS_ENDPOINT("background", "backgroundInfo", User, background_backgroundinfo);

static void background_set(Context &context, const BackgroundSetRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    if (system::background::has_background(context, *current_user, request.background_id)) {
        current_user->active_background = request.background_id;
        return;
    }

    throw error::IdolError(200, 720, "No such background");
}
NPPS_ENDPOINT("background", "set", User, background_set);

}  // namespace npps::game::background
