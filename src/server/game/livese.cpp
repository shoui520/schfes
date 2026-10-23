// npps4/game/livese.py
#include "app/idol.hpp"

namespace npps::game::livese {

static LiveSEInfoResponse livese_liveseinfo(Context &context) {
    // TODO
    util::log("stub: livese/liveseInfo %s", context.raw_request_data.c_str());
    return LiveSEInfoResponse::make({1, 2, 3, 4, 99});
}
NPPS_ENDPOINT("livese", "liveseInfo", User, livese_liveseinfo);

}  // namespace npps::game::livese
