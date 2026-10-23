// npps4/game/liveicon.py
#include "app/idol.hpp"

namespace npps::game::liveicon {

static LiveIconInfoResponse liveicon_liveiconinfo(Context &context) {
    // TODO
    util::log("stub: liveicon/liveiconInfo %s", context.raw_request_data.c_str());
    return LiveIconInfoResponse::make({1, 2, 3});
}
NPPS_ENDPOINT("liveicon", "liveiconInfo", User, liveicon_liveiconinfo);

}  // namespace npps::game::liveicon
