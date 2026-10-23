// npps4/game/gdpr.py
#include "app/idol.hpp"

namespace npps::game::gdpr {

static GDPRGetResponse gdpr_get(Context &context) {
    // TODO
    util::log("STUB gdpr/get %s", context.raw_request_data.c_str());
    return GDPRGetResponse::make(true, false);
}
NPPS_ENDPOINT("gdpr", "get", User, gdpr_get);

}  // namespace npps::game::gdpr
