// npps4/game/announce.py
#include "app/idol.hpp"
#include "system/reward.hpp"
#include "system/user.hpp"

namespace npps::game::announce {

static AnnounceStateResponse announce_checkstate(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    return AnnounceStateResponse::make(system::reward::count_presentbox(context, *current_user),
                                       false);  // TODO
}
NPPS_ENDPOINT("announce", "checkState", User, announce_checkstate);

}  // namespace npps::game::announce
