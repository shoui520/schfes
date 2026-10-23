// npps4/game/marathon.py
#include "app/idol.hpp"

namespace npps::game::marathon {

static MarathonInfoResponse marathon_marathoninfo(Context &context) {
    // TODO
    util::log("STUB marathon/marathonInfo %s", context.raw_request_data.c_str());
    return {};
}
NPPS_ENDPOINT("marathon", "marathonInfo", User, marathon_marathoninfo);

}  // namespace npps::game::marathon
