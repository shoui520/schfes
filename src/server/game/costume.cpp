// npps4/game/costume.py
#include "app/idol.hpp"

namespace npps::game::costume {

static CustomeListResponse costume_costumelist(Context &) {
    // TODO
    return CustomeListResponse::make(Json::array());
}
NPPS_ENDPOINT("costume", "costumeList", User, costume_costumelist);

}  // namespace npps::game::costume
