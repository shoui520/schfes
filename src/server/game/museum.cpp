// npps4/game/museum.py
#include "app/idol.hpp"
#include "system/museum.hpp"
#include "system/user.hpp"

namespace npps::game::museum {

static MuseumInfoResponse museum_info(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    return MuseumInfoResponse::make(system::museum::get_museum_info_data(context, *current_user));
}
NPPS_ENDPOINT("museum", "info", User, museum_info);

}  // namespace npps::game::museum
