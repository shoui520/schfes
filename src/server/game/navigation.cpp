// npps4/game/navigation.py
#include "app/idol.hpp"

namespace npps::game::navigation {

static NavigationSpecialCutInResponse navigation_specialcutin(Context &context) {
    // TODO
    util::log("STUB navigation/specialCutin %s", context.raw_request_data.c_str());
    return NavigationSpecialCutInResponse::make(Json::array());
}
NPPS_ENDPOINT("navigation", "specialCutin", User, navigation_specialcutin);

}  // namespace npps::game::navigation
