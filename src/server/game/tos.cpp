// npps4/game/tos.py
#include "app/idol.hpp"
#include "system/tos.hpp"
#include "system/user.hpp"

namespace npps::game::tos {

static TOSCheckResponse tos_toscheck(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    bool agree = system::tos::is_agreed(context, *current_user, 1);
    return TOSCheckResponse::make(1, 1, agree);
}
NPPS_ENDPOINT("tos", "tosCheck", User, tos_toscheck);

static void tos_tosagree(Context &context, const TOSAgreeRequest &request) {
    if (request.tos_id == 1) {
        db::main::User *current_user = system::user::get_current(context);
        if (!system::tos::is_agreed(context, *current_user, 1)) system::tos::agree(context, *current_user, 1);

        return;
    }

    throw error::IdolError(200, 720, "Invalid ToS agreement");
}
NPPS_ENDPOINT("tos", "tosAgree", User, tos_tosagree, .batchable = false);

}  // namespace npps::game::tos
